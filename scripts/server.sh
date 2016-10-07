


waitclients(){
  echo -n | nc -ld 5000 -k &
  p=$!
  read -p "Press any key to start $1... " -n1 -s
  kill $p
}

cd ..
port=8000

waitclients "test1.128"     | timeout 5m ./echo_server -p $port -s 128     | sed "s/^/[test1 128    ] /" > logs/server.teste1.128.txt
waitclients "test1.1024"    | timeout 5m ./echo_server -p $port -s 1024    | sed "s/^/[test1 1024   ] /" > logs/server.teste1.1024.txt
waitclients "test1.8192"    | timeout 5m ./echo_server -p $port -s 8192    | sed "s/^/[test1 8192   ] /" > logs/server.teste1.8192.txt
waitclients "test1.65536"   | timeout 5m ./echo_server -p $port -s 65536   | sed "s/^/[test1 65536  ] /" > logs/server.teste1.65536.txt
waitclients "test1.524288"  | timeout 5m ./echo_server -p $port -s 524288  | sed "s/^/[test1 524288 ] /" > logs/server.teste1.524288.txt
waitclients "test1.4194304" | timeout 5m ./echo_server -p $port -s 4194304 | sed "s/^/[test1 4194304] /" > logs/server.teste1.4194304.txt
