package httpclient

import (
	"bytes"
	"encoding/gob"
	"io/ioutil"
	"net/http"
	"niovakv/niovakvlib"

	log "github.com/sirupsen/logrus"
)

func serviceRequest(req *http.Request) (*niovakvlib.NiovaKVResponse, error) {

	responseObj := niovakvlib.NiovaKVResponse{}
	req.Header.Set("Content-Type", "application/json; charset=utf-8")
	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		log.Error(err)
		return &responseObj, err
	}
	/*
		Check if response if ok,
		If not, fill the response obj with desired values
		If so, fill the resp obj with received values
	*/
	log.Info("HTTP response status : ", resp.Status)
	if resp.StatusCode == 200 {
		//Serviced
		defer resp.Body.Close()
		bodyBytes, _ := ioutil.ReadAll(resp.Body)
		//Unmarshal the response.
		dec := gob.NewDecoder(bytes.NewBuffer(bodyBytes))
		err = dec.Decode(&responseObj)
		if err == nil {
			log.Info("Status of operation for key :", responseObj.RespStatus)
		}
	} else if resp.StatusCode == 503 {
		//Service not found
		responseObj.RespStatus = -1
		responseObj.RespValue = []byte("Server timed out")
	} else {
		responseObj.RespStatus = -1
		responseObj.RespValue = []byte(resp.Status)
	}
	return &responseObj, err
}

func PutRequest(reqobj *niovakvlib.NiovaKV, addr, port string) (*niovakvlib.NiovaKVResponse, error) {
	var request bytes.Buffer
	enc := gob.NewEncoder(&request)
	err := enc.Encode(reqobj)
	if err != nil {
		log.Error(err)
		return nil, err
	}
	conn_addr := "http://" + addr + ":" + port
	req, err := http.NewRequest(http.MethodPut, conn_addr, bytes.NewBuffer(request.Bytes()))
	if err != nil {
		log.Error(err)
		return nil, err
	}
	return serviceRequest(req)
}

func GetRequest(reqobj *niovakvlib.NiovaKV, addr, port string) (*niovakvlib.NiovaKVResponse, error) {
	var request bytes.Buffer
	enc := gob.NewEncoder(&request)
	err := enc.Encode(reqobj)
	if err != nil {
		log.Error(err)
		return nil, err
	}

	conn_addr := "http://" + addr + ":" + port
	req, err := http.NewRequest(http.MethodGet, conn_addr, bytes.NewBuffer(request.Bytes()))
	if err != nil {
		log.Error(err)
		return nil, err
	}
	return serviceRequest(req)
}
