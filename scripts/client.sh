
ip=$1

waitserver(){
  read -p "Press any key to start $1... " -n1 -s
  echo "waitng server to start $1"
  nc $ip 5000
  sleep 1
  echo "starting $1"
}

cd ..
port=8000

waitserver "test1.128"     | timeout 5m ./flood_client -h $ip -p $port -s 128     | sed "s/^/[test1 128    ] /" > logs/cliente.teste1.128.txt
waitserver "test1.1024"    | timeout 5m ./flood_client -h $ip -p $port -s 1024    | sed "s/^/[test1 1024   ] /" > logs/cliente.teste1.1024.txt
waitserver "test1.8192"    | timeout 5m ./flood_client -h $ip -p $port -s 8192    | sed "s/^/[test1 8192   ] /" > logs/cliente.teste1.8192.txt
waitserver "test1.65536"   | timeout 5m ./flood_client -h $ip -p $port -s 65536   | sed "s/^/[test1 65536  ] /" > logs/cliente.teste1.65536.txt
waitserver "test1.524288"  | timeout 5m ./flood_client -h $ip -p $port -s 524288  | sed "s/^/[test1 524288 ] /" > logs/cliente.teste1.524288.txt
waitserver "test1.4194304" | timeout 5m ./flood_client -h $ip -p $port -s 4194304 | sed "s/^/[test1 4194304] /" > logs/cliente.teste1.4194304.txt
