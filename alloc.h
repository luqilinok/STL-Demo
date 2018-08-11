#if 0
#include<new>
#define _THROW_BAD_ALLOC throw bad_alloc
#elif !defined(_THROW_BAD_ALLOC)
#include<iostream>
#define _THROW_BAD_ALLOC cerr<<"out of memory"<<endl;exit(1)
#endif // 0

//第一级配置器
template<int inst>
class _malloc_alloc_template
{
private:
	static void *oom_malloc(size_t);
	static void *oom_realloc(void*, size_t);
	static void(*_malloc_alloc_oom_handler)();
public:
	static void* allocate(size_t n)
	{
		void *result = malloc(n);
		if (0 == result) result = oom_malloc(n);
		return result;
	}
	static void deallocate(void *p, size_t)
	{
		free(p);
	}
	static void *reallocate(void *p, size_t, size_t new_sz)
	{
		void *result = realloc(p, new_sz);
		if (0 == result) result = oom_realloc(p, new_sz);
		return result;
	}

	static void(*set_malloc_handler(void(*f)()))()
	{
		void(*old)() = _malloc_alloc_oom_handler;
		_malloc_alloc_oom_handler = f;
		return(old);
	}
};

template<int inst>
void(*_malloc_alloc_template<inst>::_malloc_alloc_oom_handler)() = 0;

template<int inst>
void *_malloc_alloc_template<inst>::oom_malloc(size_t n)
{
	void(*my_malloc_handler)();
	void *result;

	for (;;)
	{
		my_malloc_handler = _malloc_alloc_oom_handler;
		if (0 == my_malloc_handler) { _THROW_BAD_ALLOC; }
		(*my_malloc_handler)();
		result = malloc(n);
		if (result)return(result);
	}
}

template<int inst>
void *_malloc_alloc_template<inst>::oom_realloc(void *p, size_t n)
{
	void(*my_malloc_handler)();
	void *result;

	for (;;)
	{
		my_malloc_handler = _malloc_alloc_oom_handler;
		if (0 == my_malloc_handler) { _THROW_BAD_ALLOC; }
		(*my_malloc_handler)();
		result = realloc(p, n);
		if (result) return(result);
	}
}

//第二级空间配置器
enum { _ALIGN = 8 };
enum { _MAX_BYTES = 128 };
enum { _NFREELISTS = _MAX_BYTES / _ALIGN };

template<bool threads, int inst>
class _default_alloc_template
{
private:
	static size_t ROUND_UP(size_t bytes)
	{
		return(((bytes)+_ALIGN - 1)&~(_ALIGN - 1));
	}
private:
	union obj
	{
		union obj * free_list_link;
		char client_data[1];
	};
private:
	static obj *volatile free_list[_NFREELISTS];
	static size_t FREELIST_INDEX(size_t bytes)
	{
		return(((bytes)+_ALIGN - 1) / _ALIGN - 1);
	}
	static void *refill(size_t n);
	static char *chunk_alloc(size_t size, int &nobjs);

	static char *start_free;
	static char *end_free;
	static size_t heap_size;
public:
	static void * allocate(size_t n) {}
	static void deallocate(void *p, size_t n) {}
	static void * reallocate(void *p, size_t old_sz, size_t new_sz);
};

//static data member的定义与初值设定
template<bool threads, int inst>
char *_default_alloc_template<threads, inst>::start_free = 0;

template<bool threads, int inst>
char *_default_alloc_template<threads, inst>::end_free = 0;

template<bool threads, int inst>
size_t _default_alloc_template<threads, inst>::heap_size = 0;

template<bool threads, int inst>
_default_alloc_template<threads, inst>::obj *volatile
_default_alloc_template<threads, inst>::free_list[_NFREELISTS] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

//空间配置函数allocate()
template<bool threads, int inst>
inline static void* _default_alloc_template<threads, inst>::allocate(size_t n)
{
	obj * volatile *my_free_list;
	obj * result;

	if (n > (size_t)_MAX_BYTES)
	{
		return(malloc_alloc::allocate(n));
	}
	my_free_list = free_list + FREELIST_INDEX(n);
	result = *my_free_list;
	if (result == 0)
	{
		void *r = refill(ROUND_UP(n));
		return r;
	}
	*my_free_list = result->free_list_link;
	return(result);
};

//空间释放函数deallocate()
template<bool threads, int inst>
inline static void _default_alloc_template<threads, inst>::deallocate(void *p, size_t n)
{
	obj *q = (obj*)p;
	obj * volatile *my_free_list;

	if (n > (size_t)_MAX_BYTES)
	{
		malloc_alloc::deallocate(p, n);
		return;
	}
	my_free_list = free_list + FREELIST_INDEX(n);
	q->free_list_link = *my_free_list;
	*my_free_list = q;
}

//重新填充free list
template<bool threads, int inst>
inline void * _default_alloc_template<threads, inst>::refill(size_t n)
{
	int nobjs = 20;
	char * chunk = chunk_alloc(n, nobjs);
	obj * volatile * my_free_list;
	obj * result;
	obj * current_obj, *next_obj;
	int i;
	if (1 == nobjs)
	{
		return(chunk);
	}
	my_free_list = free_list + FREELIST_INDEX(n);
	result = (obj*)chunk;
	*my_free_list = next_obj = (obj*)(chunk + n);
	for (i = 1;; i++)
	{
		current_obj = next_obj;
		next_obj = (obj*)((char*)next_obj + n);
		if (nobjs - 1 == i)
		{
			current_obj->free_list_link = 0;
			break;
		}
		else
		{
			current_obj->free_list_link = next_obj;
		}
	}
	return (result);
}

//内存池
