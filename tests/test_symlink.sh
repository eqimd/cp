mkdir tests/tmp
cp cp tests/tmp
cp main.cpp tests/tmp
cd tests/tmp

ln -s main.cpp mainlink

./cp mainlink testlink &&
ls . | grep testlink &&
test -L testlink &&
test $(readlink -f mainlink) = $(readlink -f testlink) &&
cd .. &&
rm -r tmp