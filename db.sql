CREATE TABLE nginx.nginx_streamer
(
    requestDate Date, 
    requestDateTime DateTime, 
    remote_addr String, 
    body_bytes_sent UInt64, 
    request_time Float32, 
    status String, 
    request String, 
    request_method String, 
    http_user_agent String
) ENGINE = MergeTree(requestDate, (remote_addr, status, request), 8192)
