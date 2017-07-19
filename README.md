# nginx2clickhouse
Parsing and sending nginx logs to clickhouse db

# Logging.

First we have to adjust Nginx config file for JSON log output in /vi/etc/nginx/nginx.conf

We have to edit http section:

```
http {
...
         log_format test '{ "requestDate": "$time_iso8601", '
         '"requestDateTime": "$time_iso8601", '
         '"remote_addr": "$remote_addr", '
         '"body_bytes_sent": "$body_bytes_sent", '
         '"request_time": "$request_time", '
         '"status": "$status", '
         '"request": "$request", '
         '"request_method": "$request_method", '
         '"http_user_agent": "$http_user_agent" }'; 
...
         }
```
After this, edit vi /etc/nginx/sites-enabled/test.conf inside server section:
```
  server {
  ...
  access_log /var/log/nginx/tv.log test;
  ...
  }
```

#Parsing

Compile feeder.c with GCC. It will produce neu JSON with Date and DateTime field

Before:
```
{ "requestDate": "2017-07-12T15:14:33+03:00", "requestDateTime": "2017-07-12T15:14:33+03:00",
```
After parsing:
```
{ "requestDate": "2017-07-12", "requestDateTime": "2017-07-12 15:14:33",
```

We have to parse JSON that way because ClickHouse-Grafana datasource works only with Date | DateTime values with no ISO or UNIX time support.
