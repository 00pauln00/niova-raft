module niovakv/niovaclient

replace niovakv/serfclienthandler => ../serf/client

replace niovakv/httpclient => ../http/client

replace niovakv/niovakvlib => ../lib

go 1.16

require (
	niovakv/httpclient v0.0.0-00010101000000-000000000000
	niovakv/niovakvlib v0.0.0-00010101000000-000000000000
	niovakv/serfclienthandler v0.0.0-00010101000000-000000000000
)
