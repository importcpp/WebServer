#include <list>
#include <unordered_map>

using namespace std;

class shared_ptr
{
private:
    int *cnt_;
    int *ptr_;

public:
    shared_ptr(int *ptr) : cnt_(new int), ptr_(ptr) {}
    shared_ptr(const shared_ptr &rhs)
    {
        (*rhs.cnt_)++;
        this->cnt_ = rhs.cnt_;
        this->ptr_ = rhs.ptr_;
    }

    shared_ptr &operator=(const shared_ptr &rhs)
    {
        (*rhs.cnt_)++;
        this->cnt_ = rhs.cnt_;
        this->ptr_ = rhs.ptr_;
        return *this;
    }

    ~shared_ptr()
    {
        if (*cnt_ == 0)
        {
            release();
        }
        else
        {
            ptr_ = nullptr;
        }
    }

    int use_count() const
    {
        return *cnt_;
    }

    void release()
    {
        if (*cnt_ == 0)
        {
            delete cnt_;
            cnt_ = nullptr;
            delete ptr_;
            ptr_ = nullptr;
        }
        else
        {
            ptr_ = nullptr;
        }
    }
};

int main()
{
}