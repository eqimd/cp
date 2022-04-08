#include <sys/types.h>
#include<string>

class FileDescriptor {
public:
    explicit FileDescriptor(const std::string& fn);

    ~FileDescriptor();
    int getDescriptor() const;
    size_t getFileSize() const;
    void copyTo(const std::string& path);

private:
    int descr;

    void copyDirectly(int destDescr);
    void copyHardlink(int destDescr);
    void copySymlink(int destDescr);
};