
// 1. 遍历链表  safe
// safe是在遍历链表时会保存pos的下一个节点
// linux默认只遍历不删除，用list_for_each，
// 若在list_for_each里删除了节点那会找不到下一个节点了，
// 循环链表，所以循环结束条件就是 pos != (head)
#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)


// 2. 遍历中的 1;
// 当pos为null时n上次都为null了, 因此当pos为最后一个节点时pos->next为null，但n也要保存
// 判断条件为pos不为空还不行，还要把pos->next存入n，pos为空肯定结束遍历，不为空时还要存pos->next到n，
// 因此： pos && ({ n = pos->next; 1; });
//      { n = pos->next; 1; } 永远为真，
//      与逗号表式一样，从左到右执行，整个表达式的值是最后一个语句 1; 只要pos还有pos->next一定保存到n
//      要满足遍历每个节点的宏hlist_for_each，则 1;很骚
#define hlist_for_each_safe(pos, n, head) \
	for (pos = (head)->first; pos && ({ n = pos->next; 1; }); \
	     pos = n)

#define hlist_for_each_entry(tpos, pos, head, member)			 \
	for (pos = (head)->first;					 \
	     pos &&							 \
		({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
	     pos = pos->next)