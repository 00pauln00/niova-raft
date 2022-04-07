package PumiceDBServer

import (
	"errors"
	"fmt"
	"reflect"
	"strconv"
	"unsafe"
	"niova/go-pumicedb-lib/common"
	log "github.com/sirupsen/logrus"

	gopointer "github.com/mattn/go-pointer"
)
/*
#cgo LDFLAGS: -lniova -lniova_raft -lniova_pumice -lrocksdb
#include <raft/pumice_db.h>
#include <rocksdb/c.h>
#include <raft/raft_net.h>
#include <raft/pumice_db_client.h>
extern void applyCgo(const struct raft_net_client_user_id *, const void *,
                     size_t, void *, void *);
extern size_t readCgo(const struct raft_net_client_user_id *, const void *,
                    size_t, void *, size_t, void *);
	*/
import "C"


type PmdbServerAPI interface {
	Apply(unsafe.Pointer, unsafe.Pointer, int64, unsafe.Pointer)
	Read(unsafe.Pointer, unsafe.Pointer, int64, unsafe.Pointer, int64) int64
}

type PmdbServerObject struct {
	PmdbAPI        PmdbServerAPI
	RaftUuid       string
	PeerUuid       string
	ColumnFamilies string // XXX should be an array of strings
	SyncWrites     bool
	CoalescedWrite bool
}


type charsSlice []*C.char

/* Typecast Go string to C String */
func GoToCString(gstring string) *C.char {
	return C.CString(gstring)
}

/* Free the C memory */
func FreeCMem(cstring *C.char) {
	C.free(unsafe.Pointer(cstring))
}

/* Typecast Go Int to string */
func GoIntToString(value int) string {
	return strconv.Itoa(value)
}

/* Get length of the Go string */
func GoStringLen(str string) int {
	return len(str)
}

/* Type cast Go int64 to C size_t */
func GoToCSize_t(glen int64) C.size_t {
	return C.size_t(glen)
}

/* Typecast C size_t to Go int64 */
func CToGoInt64(cvalue C.size_t) int64 {
	return int64(cvalue)
}

/* Type cast C char * to Go string */
func CToGoBytes(C_value *C.char, C_value_len C.int) []byte {
	return C.GoBytes(unsafe.Pointer(C_value), C_value_len)
}

/*
 The following goApply and goRead functions are the exported
 functions which is needed for calling the golang function
 pointers from C.
*/

//export goApply
func goApply(app_id *C.struct_raft_net_client_user_id, input_buf unsafe.Pointer,
	input_buf_sz C.size_t, pmdb_handle unsafe.Pointer,
	user_data unsafe.Pointer) {

	//Restore the golang function pointers stored in PmdbCallbacks.
	gcb := gopointer.Restore(user_data).(*PmdbServerObject)

	//Convert buffer size from c data type size_t to golang int64.
	input_buf_sz_go := CToGoInt64(input_buf_sz)

	//Calling the golang Application's Apply function.
	gcb.PmdbAPI.Apply(unsafe.Pointer(app_id), input_buf, input_buf_sz_go,
		pmdb_handle)
}

//export goRead
func goRead(app_id *C.struct_raft_net_client_user_id, request_buf unsafe.Pointer,
	request_bufsz C.size_t, reply_buf unsafe.Pointer, reply_bufsz C.size_t,
	user_data unsafe.Pointer) int64 {

	//Restore the golang function pointers stored in PmdbCallbacks.
	gcb := gopointer.Restore(user_data).(*PmdbServerObject)

	//Convert buffer size from c data type size_t to golang int64.
	request_bufsz_go := CToGoInt64(request_bufsz)
	reply_bufsz_go := CToGoInt64(reply_bufsz)

	//Calling the golang Application's Read function.
	return gcb.PmdbAPI.Read(unsafe.Pointer(app_id), request_buf, request_bufsz_go,
		reply_buf, reply_bufsz_go)
}

/**
 * Start the pmdb server.
 * @raft_uuid: Raft UUID.
 * @peer_uuid: Peer UUID.
 * @cf: Column Family
 * @cb: PmdbAPI callback funcs.
 */
func PmdbStartServer(pso *PmdbServerObject) error {

	if pso == nil {
		return errors.New("Null server object parameter")
	}
	if pso.RaftUuid == "" || pso.PeerUuid == "" {
		return errors.New("Raft and/or peer UUIDs were not specified")
	}

	/*
	 * Convert the raft_uuid and peer_uuid go strings into C strings
	 * so that we can pass these to C function.
	 */

	raft_uuid_c := GoToCString(pso.RaftUuid)
	defer FreeCMem(raft_uuid_c)

	peer_uuid_c := GoToCString(pso.PeerUuid)
	defer FreeCMem(peer_uuid_c)

	cCallbacks := C.struct_PmdbAPI{}

	//Assign the callback functions for apply and read
	cCallbacks.pmdb_apply = C.pmdb_apply_sm_handler_t(C.applyCgo)
	cCallbacks.pmdb_read = C.pmdb_read_sm_handler_t(C.readCgo)

	/*
	 * Store the column family name into char * array.
	 * Store gostring to byte array.
	 * Don't forget to append the null terminating character.
	 */
	cf_byte_arr := []byte(pso.ColumnFamilies + "\000")

	cf_name := make(charsSlice, len(cf_byte_arr))
	cf_name[0] = (*C.char)(C.CBytes(cf_byte_arr))

	//Convert Byte array to char **
	sH := (*reflect.SliceHeader)(unsafe.Pointer(&cf_name))
	cf_array := (**C.char)(unsafe.Pointer(sH.Data))

	// Create an opaque C pointer for cbs to pass to PmdbStartServer.
	opa_ptr := gopointer.Save(pso)
	defer gopointer.Unref(opa_ptr)

	// Starting the pmdb server.
	rc := C.PmdbExec(raft_uuid_c, peer_uuid_c, &cCallbacks, cf_array, 1,
		(C.bool)(pso.SyncWrites), (C.bool)(pso.CoalescedWrite), opa_ptr)

	if rc != 0 {
		return fmt.Errorf("PmdbExec() returned %d", rc)
	}

	return nil
}

// Method version of PmdbStartServer()
func (pso *PmdbServerObject) Run() error {
	return PmdbStartServer(pso)
}

// Export the common decode method via the server object
func (*PmdbServerObject) Decode(input unsafe.Pointer, output interface{},
	len int64) error {
	return PumiceDBCommon.Decode(input, output, len)
}

// search a key in RocksDB
func PmdbLookupKey(key string, key_len int64,
	go_cf string) ([]byte, error) {

	var goerr string
	var C_value_len C.size_t
	var result []byte
	var lookup_err error

	err := GoToCString(goerr)

	cf := GoToCString(go_cf)

	//Convert go string to C char *
	C_key := GoToCString(key)

	C_key_len := GoToCSize_t(key_len)

	//Get the column family handle
	cf_handle := C.PmdbCfHandleLookup(cf)

	ropts := C.rocksdb_readoptions_create()

	C_value := C.rocksdb_get_cf(C.PmdbGetRocksDB(), ropts, cf_handle, C_key,
		C_key_len, &C_value_len, &err)

	C.rocksdb_readoptions_destroy(ropts)

	if C_value != nil {

		buffer_value := CToGoBytes(C_value, C.int(C_value_len))

		log.Debug("C_value: ", C_value, " \nbuffer_value: ", buffer_value)

		result = C.GoBytes(unsafe.Pointer(C_value), C.int(C_value_len))
		lookup_err = nil
		FreeCMem(C_value)
	} else {
		lookup_err = errors.New("Failed to lookup for key")
	}

	//Free C memory
	FreeCMem(err)
	FreeCMem(cf)
	FreeCMem(C_key)
	log.Info("Result is :",result)
	return result, lookup_err
}

// Public method of PmdbLookupKey
func (*PmdbServerObject) LookupKey(key string, key_len int64,
	go_cf string) ([]byte, error) {
	return PmdbLookupKey(key, key_len, go_cf)
}

func PmdbWriteKV(app_id unsafe.Pointer, pmdb_handle unsafe.Pointer, key string,
	key_len int64, value string, value_len int64, gocolfamily string) {

	//typecast go string to C char *
	cf := GoToCString(gocolfamily)

	C_key := GoToCString(key)
	log.Info("WRITE - c_key", C_key)

	C_key_len := GoToCSize_t(key_len)

	C_value := GoToCString(value)
	log.Info("WRITE - c_value", C_value)

	C_value_len := GoToCSize_t(value_len)

	capp_id := (*C.struct_raft_net_client_user_id)(app_id)

	cf_handle := C.PmdbCfHandleLookup(cf)

	//Calling pmdb library function to write Key-Value.
	rc := C.PmdbWriteKV(capp_id, pmdb_handle, C_key, C_key_len, C_value, C_value_len, nil, unsafe.Pointer(cf_handle))

	go_rc := int(rc)
	if go_rc != 0 {
		log.Error("PmdbWriteKV failed with error: ", go_rc)
	}
	//Free C memory
	FreeCMem(cf)
	FreeCMem(C_key)
	FreeCMem(C_value)
}

// Public method of PmdbWriteKV
func (*PmdbServerObject) WriteKV(app_id unsafe.Pointer,
	pmdb_handle unsafe.Pointer, key string,
	key_len int64, value string, value_len int64, gocolfamily string) {

	PmdbWriteKV(app_id, pmdb_handle, key, key_len, value, value_len,
		gocolfamily)
}

func PmdbReadKV(app_id unsafe.Pointer, key string,
	key_len int64, gocolfamily string) ([]byte, error) {

	go_value, err := PmdbLookupKey(key, key_len, gocolfamily)

	//Get the result
	return go_value, err
}

// Public method of PmdbReadKV
func (*PmdbServerObject) ReadKV(app_id unsafe.Pointer, key string,
	key_len int64, gocolfamily string) ([]byte, error) {

	return PmdbReadKV(app_id, key, key_len, gocolfamily)
}

// Methods for range iterator

func byteToChar(b []byte) *C.char {
	var c *C.char
	if len(b) > 0 {
		c = (*C.char)(unsafe.Pointer(&b[0]))
	}
	return c
}

func PmdbRangeLookupKey(key string, key_len int64,
	go_cf string) ([]byte, error) {
	var lookup_err error
	// var ptr unsafe.Pointer
	cKey := GoToCString(key)
	cLen := GoToCSize_t(key_len)
	var cLenVal C.size_t
	var cLenKey C.size_t

	//(C.PmdbGetRocksDB(),
	ropts := C.rocksdb_readoptions_create()
	log.Info("Key passed is:", key)
	log.Info("cKey is :", cKey)

	itr := C.rocksdb_create_iterator(C.PmdbGetRocksDB(), ropts)
	log.Info("Inside pmdbRangeLookupKey")
	//itr := NewNativeIterator(ptr)

	C.rocksdb_iter_seek(itr, cKey, cLen)
	log.Info("after seek")
	//itr.Seek(key)
	cResultKey := C.rocksdb_iter_key(itr, &cLenKey)
	log.Info("Between key and val")
	cResultVal := C.rocksdb_iter_value(itr, &cLenVal)
	//result = []byte(itr.Value())
	log.Info("Printing range key here ",C.GoString(cResultKey))
	log.Info("Printing range value here",C.GoString(cResultVal))
	log.Info("Key is :", string(CToGoBytes(cResultKey, C.int(cLenKey))))
	log.Info("Value is :", string(CToGoBytes(cResultVal, C.int(cLenVal))))
	return C.GoBytes(unsafe.Pointer(cResultVal), C.int(cLenVal)) ,lookup_err

}

func PmdbRangeReadKV(app_id unsafe.Pointer, key string,
	key_len int64, gocolfamily string) ([]byte, error) {
	log.Info("Inside pmdbRangeReadKV")
	go_value, err := PmdbRangeLookupKey(key, key_len, gocolfamily)
	//Get the result
	return go_value, err
}

func (*PmdbServerObject) RangeReadKV(app_id unsafe.Pointer, key string,
	key_len int64, gocolfamily string) ([]byte, error) {
	log.Info("Inside RangeReadKV")
	return PmdbRangeReadKV(app_id, key, key_len, gocolfamily)
}

// Copy data from the user's application into the pmdb reply buffer
func PmdbCopyDataToBuffer(ed interface{}, buffer unsafe.Pointer) (int64, error) {
	var key_len int64
	//Encode the structure into void pointer.
	encoded_key, err := PumiceDBCommon.Encode(ed, &key_len)
	if err != nil {
		log.Print("Failed to encode data during copy data: ", err)
		return -1, err
	}

	//Copy the encoded structed into buffer
	C.memcpy(buffer, encoded_key, C.size_t(key_len))

	return key_len, nil
}

// Public method version of PmdbCopyDataToBuffer
func (*PmdbServerObject) CopyDataToBuffer(ed interface{},
	buffer unsafe.Pointer) (int64, error) {
	return PmdbCopyDataToBuffer(ed, buffer)
}
