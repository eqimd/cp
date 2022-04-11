dd bs=1024 count=1000000 < /dev/urandom > /dev/shm/test_tmp

./cp /dev/shm/test_tmp . && cmp --silent /dev/shm/test_tmp test_tmp
