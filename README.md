Specify the port and origin URL to start the cache. 
The expiration times can be tested using a site like https://httpbin.org.

Example commands in PowerShell in the VS Code terminal:
curl.exe -i http://localhost:8100/cache/10
curl.exe -i -H "Accept: application/json" http://localhost:8100/stats
curl.exe -i http://localhost:8100/stats