#include <types.h>
#include <lib.h>
#include <synch.h>
#include <test.h>
#include <thread.h>

#include "paintshop.h"
#include "paintshop_driver.h"


/*
 * **********************************************************************
 * YOU ARE FREE TO CHANGE THIS FILE BELOW THIS POINT AS YOU SEE FIT
 *
 */

/* Declare any globals you need here (e.g. locks, etc...) */
#define ORDER_BUFFER_SIZE 10
#define SHIP_BUFFER_SIZE 10

#define TRUE 1
#define FALSE 0

//extern static int customers;

static struct semaphore *order_mutex, *order_empty, *order_full;

static struct semaphore *ship_mutex, *ship_empty, *ship_full;

static struct semaphore *tints_mutex;

struct{
	struct paintorder *elements[ORDER_BUFFER_SIZE];
	int first; /* buf[first%BUFF_SIZE] is the first empty slot  */
	int last; //buf[last%BUFF_SIZE] is the first full slot
}order_buffer;

void *ship_buffer[SHIP_BUFFER_SIZE];//ship buffer store the point of order

bool go_home;
/*
 * **********************************************************************
 * FUNCTIONS EXECUTED BY CUSTOMER THREADS
 * **********************************************************************
 */

/*
 * order_paint()
 *
 * Takes one argument referring to the order to be filled. The
 * function makes the order available to staff threads and then blocks
 * until the staff have filled the can with the appropriately tinted
 * paint.
 */

void order_paint(struct paintorder *order)
{

	P(order_empty);
	P(order_mutex);
	order_buffer.elements[order_buffer.first] = order;
	order_buffer.first = (order_buffer.first + 1) % ORDER_BUFFER_SIZE;
	V(order_mutex);
	V(order_full);


	if((*order).go_home_flag == 1)
		return;

	bool found = FALSE;
	while(TRUE){
		P(ship_full);
		P(ship_mutex);

		for(int i=0;i<SHIP_BUFFER_SIZE;i++){
			//kprintf("ship_buffer:%p vs order: %p \t", ship_buffer[i], order);
			if(ship_buffer[i] == order){
				ship_buffer[i] = NULL;
				found = TRUE;
				break;
			}
		}
		kprintf("\n");

		V(ship_mutex);
		if(found){
			V(ship_empty);
			break;
		}else V(ship_full);
	}

}



/*
 * **********************************************************************
 * FUNCTIONS EXECUTED BY PAINT SHOP STAFF THREADS
 * **********************************************************************
 */

/*
 * take_order()
 *
 * This function waits for a new order to be submitted by
 * customers. When submitted, it returns a pointer to the order.
 *
 */

struct paintorder *take_order(void)
{
	P(order_full);
	P(order_mutex);

	struct paintorder *ret = order_buffer.elements[order_buffer.last];
	order_buffer.last = (order_buffer.last + 1) % ORDER_BUFFER_SIZE;

	V(order_mutex);
	V(order_empty);
	return ret;

}


/*
 * fill_order()
 *
 * This function takes an order provided by take_order and fills the
 * order using the mix() function to tint the paint.
 *
 * NOTE: IT NEEDS TO ENSURE THAT MIX HAS EXCLUSIVE ACCESS TO THE
 * REQUIRED TINTS (AND, IDEALLY, ONLY THE TINTS) IT NEEDS TO USE TO
 * FILL THE ORDER.
 */

void fill_order(struct paintorder *order)
{
    /* add any sync primitives you need to ensure mutual exclusion
           holds as described */
	P(tints_mutex);
		/* the call to mix must remain */
		mix(order);
	V(tints_mutex);
}


/*
 * serve_order()
 *
 * Takes a filled order and makes it available to (unblocks) the
 * waiting customer.
 */

void serve_order(struct paintorder *order)
{
	if(go_home)
		return;

	if((*order).go_home_flag){
		go_home = 1;
		return;
	}


	P(ship_empty);
	P(ship_mutex);

	for(int i=0;i<ORDER_BUFFER_SIZE;i++){
		if(ship_buffer[i] == NULL){
			ship_buffer[i] = order;
			break;
		}
	}
	V(ship_mutex);
	V(ship_full);
}



/*
 * **********************************************************************
 * INITIALISATION AND CLEANUP FUNCTIONS
 * **********************************************************************
 */


/*
 * paintshop_open()
 *
 * Perform any initialisation you need prior to opening the paint shop
 * to staff and customers. Typically, allocation and initialisation of
 * synch primitive and variable.
 */


void paintshop_open(void)
{
	order_mutex = sem_create("order_mutex", 1);
	order_empty = sem_create("order_empty", ORDER_BUFFER_SIZE);
	order_full = sem_create("order_full", 0);

	ship_empty = sem_create("ship_empty", SHIP_BUFFER_SIZE);
	ship_full = sem_create("ship_full", 0);
	ship_mutex = sem_create("ship_mutex", 1);

	tints_mutex = sem_create("tints_mutex", 1);

}

/*
 * paintshop_close()
 *
 * Perform any cleanup after the paint shop has closed and everybody
 * has gone home.
 */

void paintshop_close(void)
{
	sem_destroy(order_mutex);
	sem_destroy(order_empty);
	sem_destroy(order_full);
	sem_destroy(ship_empty);
	sem_destroy(ship_full);
	sem_destroy(ship_mutex);
	sem_destroy(tints_mutex);

}
