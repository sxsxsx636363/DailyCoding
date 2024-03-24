
#include <iostream>
#include <algorithm>
#include <vector>
#include <cstdlib>
#include <queue>
#include <thread>
#include <mutex>
#include <functional>
#include <atomic>

using namespace std;
class Tasknode{
public:
    virtual int Run() = 0;
    function<bool()> Exit = NULL;
};

class Threadpool{
private:
    int thread_num = 0;
    mutex mux;
    vector<thread*> threadvec;
    queue<Tasknode*> tasklist;//任务队列
    bool isExit = false;//线程池退出
    atomic<int> running_task = {0};//正在运行的任务数量，线程安全
    
    condition_variable cv;
    void Run();//线程池线程的入口函数
public:
    //初始化线程池
    void init(int threadnum){
        unique_lock<mutex> lock(mux);
        thread_num = threadnum;
    }
    
    //启动线程
    void start(int threadnum){
        init(threadnum);
        
        unique_lock<mutex> lock(mux);
        if(thread_num <= 0){
            cerr << "No thread exist" << endl;
            return;
        }
        
        if(!threadvec.empty()){
            cerr << "Thread pool have already started" << endl;
            return;
        }
        
        for(int i = 0; i < thread_num; ++i){//创建线程
            auto newthread = new thread(&Threadpool:: Run, this);//成员函数指针, 对象实例
            threadvec.push_back(newthread);
        }
    }
    
    //停止线程池
    void Stop(){
        isExit = true;
        cv.notify_all();//唤醒所有在cv上等待的线程
        for (auto& th : threadvec){//回收所有线程
            th->join();
        }
        unique_lock<mutex> lock(mux);
        threadvec.clear();
    }

    bool exit(){
        return isExit;
    }
    int count_run_tasks(){
        return running_task;
    }
    
    void AddTask(Tasknode* task){
        unique_lock<mutex> lock(mux);
        tasklist.push(task);
        task->Exit = [this]{return exit();};
        lock.unlock();
        cv.notify_one();
    }
    
    Tasknode* GetTask(){
        unique_lock<mutex> lock(mux);
        if(tasklist.empty()){
            cv.wait(lock);//如果没有任务，就将当前线程挂起知道条件变量被通知；
        }
        if(exit()){
            return NULL;
        }
        if(tasklist.empty()){
            return NULL;
        }
           
        auto first_task = tasklist.front();
        tasklist.pop();
           
        return first_task;
    }
};

//线程入口函数
void Threadpool::Run(){
    cout << "\nCurrent thread is: " << this_thread::get_id() << endl;
    while(!exit()){
        auto task = GetTask();
        if(task){
            running_task++;
            try{
                task->Run();
            }catch(exception& exc){
                cerr << "Error happened in thread: " << this_thread::get_id() << endl;
            }
            running_task--;
        }
    }
}


class PrintTask: public Tasknode{
public:
    int Run(){
        cout << "Thread: " << this_thread::get_id() << " is running the task -----------------------------" << endl;
        for(int  i = 0; i < 10; ++i){
            if(Exit()){
                break;
            }
            cout << i << endl;
            this_thread::sleep_for(500ms);
        }
        return 0;
    }
};


int main(int argc, const char * argv[]) {

    Threadpool pool;
    pool.start(4);
    
    PrintTask task1;
    pool.AddTask(&task1);
    
    PrintTask task2;
    pool.AddTask(&task2);
    
    this_thread::sleep_for(2000ms);
    cout << "Run " << pool.count_run_tasks() << " tasks in total."<<endl;
    pool.Stop();
    cout << "Run " << pool.count_run_tasks() << " tasks in total."<<endl;
    return 0;
}

