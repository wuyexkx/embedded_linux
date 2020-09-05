
// 1. 遍历链表  safe
// safe是在遍历链表时会保存pos的下一个节点
// linux默认只遍历不删除，用list_for_each，
// 若在list_for_each里删除了节点那会找不到下一个节点了，
// 循环链表，所以循环结束条件就是 pos != (head)
/**
 * list_for_each_safe - iterate over a list safe against removal of list entry
 * @pos:	the &struct list_head to use as a loop cursor.					// ===== pos is the &struct list_head =====
 * @n:		another &struct list_head to use as temporary storage
 * @head:	the head for your list.
 */
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




// 3. list_for_each_entry 
// 	include/linux/kernel.h
// ----------------------------------------------------------------------offsetof container_of list_entry
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)			// 求出struct TYPE类型 MEMBER 的偏移量
/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 */
#define container_of(ptr, type, member) ({			\					// 求出struct TYPE指针
	const typeof(((type *)0)->member) * __mptr = (ptr);	\
	(type *)((char *)__mptr - offsetof(type, member)); })

/**
 * list_entry - get the struct for this entry
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 */
#define list_entry(ptr, type, member) \									// list_entry对container_of又封装了一下
	container_of(ptr, type, member)										
// ----------------------------------------------------------------------

/**
 * list_for_each_entry	-	iterate over list of given type
 * @pos:	the type * to use as a loop cursor.								// ===== pos is the type * =====
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 */
#define list_for_each_entry(pos, head, member)				\
	for (pos = list_entry((head)->next, typeof(*pos), member);	\		// list_entry就是container_of
	     &pos->member != (head); 	\
	     pos = list_entry(pos->member.next, typeof(*pos), member))

/**
 * list_for_each_entry_continue - continue iteration over list of given type
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 *
 * Continue to iterate over list of given type, continuing after
 * the current position.
 */
#define list_for_each_entry_continue(pos, head, member) 		\
	for (pos = list_entry(pos->member.next, typeof(*pos), member);	\	// 从pos->member.next开始遍历
	     &pos->member != (head);	\
	     pos = list_entry(pos->member.next, typeof(*pos), member))

// 3.1 带entry和不带entry的区别：
// 		带entry: pos是struct TYPE* 遍历的是结构体的指针，是找list_node塞进的那个struct
// 		不带entry: pos就是list_node

// 3.2 带continue和不带continue的区别：
// 		带continue: pos已经是具体的对象，需要从这个对象开始继续往后遍历, head在判断是否结束时用
// 		不带continue: 从head开始遍历

