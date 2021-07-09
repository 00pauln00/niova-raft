module niovakv/clientapi

replace niovakv/serfclienthandler => ../serf/client

replace niova/go-pumicedb-lib/common => ../../../common

replace niovakv/httpclient => ../http/client

replace niovakv/niovakvlib => ../lib

replace niovakv/clientapi => ./clientapi

go 1.16

require (
	github.com/sirupsen/logrus v1.8.1
	niovakv/clientapi v0.0.0-00010101000000-000000000000
	niovakv/httpclient v0.0.0-00010101000000-000000000000
	niovakv/niovakvlib v0.0.0-00010101000000-000000000000
	niovakv/serfclienthandler v0.0.0-00010101000000-000000000000
)
