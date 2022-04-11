./cp main.cpp tmp/tmp1/tmp2/tmp3/main.cpp &&
cd tmp/tmp1/tmp2/tmp3 &&
ls | grep main.cpp &&
cd .. &&
cd .. &&
cd .. &&
cd .. &&
rm -r tmp