proof of concept

tail -fn0 /var/log/nginx/access.log | \
while read line; do
echo $line | sed -e 's/T[0-9][0-9]:[0-9][0-9]:[0-9][0-9][+][0-9][0-9]:[0-9][0-9]//' -e 's/[+][0-9][0-9]:[0-9][0-9]//' -e 's/./ /62' | \
POST 'http://default:@5.101.64.66:8123/?query=INSERT INTO nginx.nginx_access FORMAT JSONEachRow'
done
