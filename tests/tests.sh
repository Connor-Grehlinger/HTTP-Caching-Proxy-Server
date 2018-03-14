#!/usr/bin/env bash
echo Proxy server is up

echo Testing basic HTTP... http://www.example.com/ with trace
curl --trace-ascii - -x http://127.0.0.1:12345 http://www.example.com/
echo Proxy log:
tail ../logs/proxy.log

echo Testing concurrent HTTP GET... 10 concurrent requests to http://www.httpbin.org/get
seq 10 | parallel -n0 "curl -v -x http://127.0.0.1:12345 http://www.httpbin.org/get"
echo Proxy log:
tail ../logs/proxy.log

echo Testing HTTP CONNECT... https://www.example.com/ and https://www.google.com/ with trace
curl --trace-ascii - -x http://127.0.0.1:12345 https://www.example.com/
echo Proxy log:
tail ../logs/proxy.log

echo Testing POST requests
curl -x http://127.0.0.1:12345 -H 'Content-Type: application/json' http://httpbin.org/post -X POST -d '{\"url\":\"http://duke.edu/\", \"ece-590\":\"is awesome/\"}'
echo Proxy log:
tail ../logs/proxy.log

echo Testing concurrent HTTP POST... 5 concurrent requests to http://www.httpbin.org/post
seq 5 | parallel -n0 "curl -x http://127.0.0.1:12345 -H 'Content-Type: application/json' http://httpbin.org/post -X POST -d '{\"url\":\"http://duke.edu/\", \"ece-590\":\"is awesome/\"}'"
echo Proxy log:
tail ../logs/proxy.log

echo Testing hits in the cache... repeated HTTP GET of www.example.com/
curl -v -x http://127.0.0.1:12345 http://www.example.com/
echo Proxy log:
tail ../logs/proxy.log
curl -v -x http://127.0.0.1:12345 http://www.example.com/
echo Proxy log:
tail ../logs/proxy.log
curl -v -x http://127.0.0.1:12345 http://www.example.com/
echo Proxy log:
tail ../logs/proxy.log

echo HTTP STRESS test... http://www.espn.com/ x5
seq 5 | parallel -n0 "curl -v -x http://127.0.0.1:12345 http://www.espn.com/"
echo Proxy log:
tail ../logs/proxy.log

echo HTTPS STRESS test... https://www.youtube.com/ x5
seq 5 | parallel -n0 "curl -v -x http://127.0.0.1:12345 https://www.youtube.com/"
echo Proxy log:
tail ../logs/proxy.log

echo CONCLUDE test
echo Please view in Firefox for more.
