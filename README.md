# nginx2clickhouse
Parsing and sending nginx logs to clickhouse db

/vi/etc/nginx/nginx.conf

inside http section:

http {
...
# Logging Settings
         ##
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

vi /etc/nginx/sites-enabled/test.conf

inside server section:

  server {
  ...
  access_log /var/log/nginx/tv.log test;
  ...
       }
