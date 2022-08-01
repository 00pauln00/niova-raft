module controlplane/lookout

replace controlplane/serfagenthandler => ../serf/agent/

replace ctlplane/client_api => ../client_api/

replace ctlplane/serfclienthandler => ../serf/client/

replace ctlplane/httpclient => ../http/client/

replace common/requestResponseLib => ../RequestResponseLib/

replace common/specificCompressionLib => ../specificCompressionLib/

replace common/prometheus_handler => ../prometheusHandler/

go 1.16

require (
	controlplane/serfagenthandler v0.0.0-00010101000000-000000000000
	ctlplane/client_api v0.0.0-00010101000000-000000000000
	github.com/fsnotify/fsnotify v1.5.1
	github.com/google/uuid v1.3.0
)

require (
	common/prometheus_handler v0.0.0-00010101000000-000000000000 // indirect
	common/requestResponseLib v0.0.0-00010101000000-000000000000 // indirect
	common/specificCompressionLib v0.0.0-00010101000000-000000000000 // indirect
	ctlplane/httpclient v0.0.0-00010101000000-000000000000 // indirect
	ctlplane/serfclienthandler v0.0.0-00010101000000-000000000000 // indirect
	github.com/armon/circbuf v0.0.0-20150827004946-bbbad097214e // indirect
	github.com/armon/go-metrics v0.0.0-20180917152333-f0300d1749da // indirect
	github.com/armon/go-radix v1.0.0 // indirect
	github.com/bgentry/speakeasy v0.1.0 // indirect
	github.com/fatih/color v1.9.0 // indirect
	github.com/google/btree v0.0.0-20180813153112-4030bb1f1f0c // indirect
	github.com/hashicorp/errwrap v1.0.0 // indirect
	github.com/hashicorp/go-immutable-radix v1.0.0 // indirect
	github.com/hashicorp/go-msgpack v0.5.3 // indirect
	github.com/hashicorp/go-multierror v1.1.0 // indirect
	github.com/hashicorp/go-sockaddr v1.0.0 // indirect
	github.com/hashicorp/go-syslog v1.0.0 // indirect
	github.com/hashicorp/golang-lru v0.5.0 // indirect
	github.com/hashicorp/logutils v1.0.0 // indirect
	github.com/hashicorp/mdns v1.0.4 // indirect
	github.com/hashicorp/memberlist v0.3.0 // indirect
	github.com/hashicorp/serf v0.9.6 // indirect
	github.com/mattn/go-colorable v0.1.6 // indirect
	github.com/mattn/go-isatty v0.0.12 // indirect
	github.com/miekg/dns v1.1.41 // indirect
	github.com/mitchellh/cli v1.1.0 // indirect
	github.com/mitchellh/mapstructure v0.0.0-20160808181253-ca63d7c062ee // indirect
	github.com/posener/complete v1.2.3 // indirect
	github.com/sean-/seed v0.0.0-20170313163322-e2103e2c3529 // indirect
	github.com/sirupsen/logrus v1.8.1 // indirect
	golang.org/x/net v0.0.0-20210410081132-afb366fc7cd1 // indirect
	golang.org/x/sys v0.0.0-20210630005230-0f9fa26af87c // indirect
)
