echo 'Testing symlink...' &&
./tests/test_symlink.sh &&
echo 'Passed' && 
echo 'Testing hardlink...' &&
./tests/test_hardlink.sh &&
echo 'Passed' &&
echo 'Testing directories...' &&
./tests/test_directories.sh &&
echo 'Passed' &&
echo 'Testing direct copying...' &&
./tests/test_directly.sh &&
echo 'Passed'
