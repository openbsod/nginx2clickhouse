CREATE TABLE nginx.nginx_streamer_buffer AS nginx_streamer
ENGINE = Buffer(nginx, nginx_streamer, 16, 10, 100, 10000, 1000000, 10000000, 100000000)

#################  version depended  ######################

CREATE TABLE nginx.nginx_streamer_buffer
( requestDate Date,
 requestDateTime DateTime,
 remote_addr String,
 body_bytes_sent UInt64,
 request_time Float32,
 status String,
 request String,
 request_method String,
 http_user_agent String)
ENGINE = Buffer(\'nginx\', \'nginx_streamer\', 16, 10, 100, 10000, 1000000, 10000000, 100000000)
