# CachingProxy

This project was developed as a homework assignment for Duke Uiversity's ECE590 Robust Server Software course by Connor 
Grehlinger and Faris Sbahi

This multi-threaded proxy caches HTTP responses to GET requests according to RFC7234 standards (i.e. cached resources are not 
served to clients unless they are considered 'fresh', and stale resources are re-validated). The proxy also provides HTTPS 
support and handles POST and CONNECT requests. The proxy features logging functionality for monitoring HTTP requests and 
responses as well as cache resources. 

This proxy can handle multiple concurrent requests from several different machines while maintaining smooth, low-latency 
performance. This performance is achieved primarily through multi-threading and the use of the POCO Net library. 


## Deploy

To deploy the proxy, use with a docker container and the following commands:

```bash
$ sudo docker-compose build
$ sudo docker-compose up
```

You can easily configure Firefox to use the HTTP caching proxy server:
1) Open menu in top right corner of the Firefox browser
2) Select "Preferences"
3) Scroll down to the "Network Proxy" section and click the "Settings..." button 
4) Choose "Manual proxy configuration" and enter the IT address of the machine running the HTTP Proxy Server and the port it's 
using (port 9000 if it's from this repository and no code has been modified)
5) You can disable local browser caching with Firefox's developer mode network settings to use only the proxy server 
