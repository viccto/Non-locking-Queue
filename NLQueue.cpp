#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
using namespace std;

typedef unsigned long data_t;

#define CAS(ptr, old_val, new_val) __sync_bool_compare_and_swap(ptr, old_val, new_val)
#define TEST_TIMES_FOR_EACH_THREAD 1000

class NLQueue
{
public:
	NLQueue()
	{
		head = new Node(-1);
		tail = head;
		head->next = NULL;
		enqueueTimes = dequeueTimes = 0;
		size = 0;
	}
	
	~NLQueue()
	{
		while(head != NULL)
		{
			Node *tmp = head->next;
			delete(head);
			head = tmp;
		}
	} 

	void enQueue(data_t x)
	{
		Node *cur = new Node(x);
		cur->next = NULL;
		
		Node *p = tail;
		while(CAS(&p->next, NULL, cur) != true)
		{
			p = tail;
		}
		// 1. 正常运行的情况下, A线程的两次CAS操作之间不可能有B线程通过第一个CAS操作, 原因是tail指针仍未更新，因此p->next一定不为NULL;
		// 2. 而若A线程在两次CAS操作之间意外退出, 则将会导致死锁, 其他线程将一直等待tail的更新. 改进方法见enQueue_new().
		CAS(&tail, p, cur);
		update(&enqueueTimes, 1);
		update(&size, 1);
	}
	
	void enQueue_new(data_t x)
	{
		Node *cur = new Node(x);
		cur->next = NULL;
		
		Node *p = tail;
		Node *old_tail = p;
		do
		{
			while(p->next != NULL)
				p = p->next;
			
		}while(CAS(&p->next, NULL, cur) != true);
		// 1. 每次操作时均使p指针指向队列尾部元素, 因此第一个CAS操作一定会成功;
		// 2. 当多个线程同时执行第二次CAS操作时, 有的线程会成功, 有的线程会失败; 因此tail指向的元素并不一定是最新的尾部元素; 
		// 3. 但这种“错误”可容忍的, 并且tail在后续的操作过程中，总会有一次被更新为正确状态;
		
		CAS(&tail, old_tail, cur);
		update(&enqueueTimes, 1);
		update(&size, 1);
	} 
	
	data_t deQueue()
	{
		Node *p = head;
		if(p->next == NULL)
		{
			cout<<"Empty Queue."<<endl;
			return EFAULT;
		}
		while(__sync_bool_compare_and_swap(&head, p, p->next) != true);
		// 1. 为避免队列为空时, head和tail指向同一个元素, 因此令head始终指向一个dummy节点, 队列的首个有效节点为head->next;
		// 2. 而tail始终指向队列中的首个元素, 队列为空时则为NULL;
		data_t result = p->next->val;
		delete(p);
		update(&dequeueTimes, 1);
		update(&size, -1);
		return result;
	}
	
	void printQ()
	{
		cout<<"Non-locking Queue information:"<<endl;
		cout<<"size: "<<size<<endl;
		cout<<"enqueueTimes: "<<enqueueTimes<<endl;
		cout<<"dequeueTimes: "<<dequeueTimes<<endl;
		cout<<"data:"<<endl;
		// print data in NLQueue
		/*Node *tmp = head->next;
		while(tmp != NULL)
		{
			cout<<tmp->val<<" -> ";
			tmp = tmp->next;
		}*/
		cout<<endl;
	}
	
private:
	class Node
	{
	public:
		Node(int x)
		{
			this->val = x;
			this->next = NULL;
		}
		
		data_t val;
		Node *next;
	};
	
	Node *head;
	Node *tail;
	unsigned long enqueueTimes;
	unsigned long dequeueTimes;
	unsigned long size;
	
	void update(unsigned long *x, long y)
	{
		int tmp = *x;
		do
		{
			tmp = *x;
		}while(!CAS(x, tmp, tmp + y));
	}
};

/*
	@times: test times
	basic funtion testing, by enqueue/dequeue for x times.
*/
void basicFunctionTest(unsigned long times)
{
	NLQueue q;
	unsigned long i = 0;
	srand(time(0));
	for(i = 0; i < times; i++)
	{
		if(rand()%2)
		{
			cout<<"enqueue: "<<i<<endl;
			q.enQueue(i);
		}
		else
		{
			data_t result = q.deQueue();
			if(result != EFAULT)
				cout<<"dequeue: "<<result<<endl;
		}
	}
	cout<<"-----------"<<endl;
	q.printQ();
}


void* do_enqueue(void *args)
{
	NLQueue* q = (NLQueue *)args;
	unsigned long i = 0;
	for(i = 0; i < TEST_TIMES_FOR_EACH_THREAD; i++)
	{
		q->enQueue_new(i);
	}
	return NULL;
}

void* do_dequeue(void *args)
{
	NLQueue* q = (NLQueue *)args;
	unsigned long i = 0;
	for(i = 0; i < TEST_TIMES_FOR_EACH_THREAD; i++)
	{
		q->deQueue();
	}
	return NULL;
}

/*
	@en: number of enqueue threads
	@de: number of dequeue threads
	@times: loop times for each thread
*/
void muti_thread_test(int en, int de)
{
	NLQueue q;
	pthread_t en_threads[en], de_threads[de];
	
	int i = 0;
	for(i = 0; i < en; i++)
	{
		pthread_create(&en_threads[i], NULL, do_enqueue, &q);
	}
	for(i = 0; i < de; i++)
	{
		pthread_create(&de_threads[i], NULL, do_dequeue, &q);
	}
	
	for(i = 0; i < en; i++)
	{
		pthread_join(en_threads[i], NULL);
	}
	for(i = 0; i < de; i++)
	{
		pthread_join(de_threads[i], NULL);
	}
	q.printQ();
}

int main()
{
	basicFunctionTest(100);
	muti_thread_test(2, 1);
	return 0;
}
