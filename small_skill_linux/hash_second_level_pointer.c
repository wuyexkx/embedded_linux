
// hash表的冲突现象，计算的存储位置与已有的相同
//  解决办法：
//          开放寻址法
//          再散列法
//          链地址法

// 讲链地址法
//  基本思想是 将相同哈希地址的记录链成一个链表, m个哈希值就有m个链表
//  用数组将这m个链表的头节点存起来, 形成一个动态结构

// 在linux内核源码中list.h位于include/linux/list.h
//  其中前半部分是关于list的, 后半部分是关于hlist
//      专门用于实现双链表的链地址法的


// hash链表节点中为什么采用一个二级指针 
//  1. 若果不用这个指针, 则变成单链表, 查找必须依次索引,浪费时间
//  2. 若采用一级指针, 则头节点的类型必须和其他节点一致,否则每次都要进行数据类型转换, 
//          并且头节点和其他节点一样包含两个指针, 而头节点只需要一个指针就够了,
//          而且头节点的个数跟存储的数据总量是一个数量级的,导致浪费一倍空间,
//  3. 两级指针避免了头节点占空间的问题,也解决了双向索引问题

struct hlist_head {
	struct hlist_node *first;
};

struct hlist_node {
	struct hlist_node *next, **pprev;
};
#define HLIST_HEAD_INIT { .first = NULL }
#define HLIST_HEAD(name) struct hlist_head name = {  .first = NULL }
#define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)
static inline void INIT_HLIST_NODE(struct hlist_node *h)
{
	h->next = NULL;
	h->pprev = NULL;
}

static inline int hlist_unhashed(const struct hlist_node *h)
{
	return !h->pprev;
}

static inline int hlist_empty(const struct hlist_head *h)
{
	return !h->first;
}

static inline void __hlist_del(struct hlist_node *n)
{
	struct hlist_node *next = n->next;
	struct hlist_node **pprev = n->pprev;
	*pprev = next;                      // 将下个节点 赋值给 上个节点的后驱节点
	if (next)
		next->pprev = pprev;
}

static inline void hlist_del(struct hlist_node *n)
{
	__hlist_del(n);
	n->next = LIST_POISON1;
	n->pprev = LIST_POISON2;
}