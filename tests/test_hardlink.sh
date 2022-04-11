mkdir tests/tmp
cp cp tests/tmp
cp main.cpp tests/tmp
cd tests/tmp

./cp main.cpp newmain.cpp &&
ls . | grep newmain.cpp &&
diff main.cpp newmain.cpp &&
cd .. &&
rm -r tmp