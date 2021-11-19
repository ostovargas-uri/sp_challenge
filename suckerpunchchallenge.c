#include <stdio.h>
#include <stdlib.h>

#define SIZE 2048       // in bytes
#define MEMORY_HEADER_SIZE 8  // in bytes
#define QUEUE_HEADER_SIZE 8   // in bytes
#define BLOCK_SIZE 16   // in bytes
#define BLOCK_DATA_SIZE 14  // in bytes

unsigned char data[SIZE];

typedef struct Header {
    unsigned short lowest_free_queue_address;
    unsigned short highest_free_block_address;
    unsigned short highest_queue_address;
    unsigned short lowest_block_address;
} Header;

typedef struct QNode
{
    unsigned short next_block_address;
    unsigned char bytes[BLOCK_DATA_SIZE];
} QNode;

typedef struct Q
{
    unsigned short first_block_address;
    unsigned short last_block_address;
    unsigned short size;
    signed char first;
    signed char last;
} Q;

/*
data[0] lowest free queue address
data[1] -
data[2] highest free block address
data[3] -
data[4] highest queue address
data[5] -
data[6] lowest block address
data[7] -
*/

void on_out_of_memory()
{
    exit(1);
}

void on_illegal_operation()
{
    exit(1);
}

void set_next_lowest_block_address()
{
    Header * header = (Header *) &data[0];
    unsigned short new_lowest_block_address = header->lowest_block_address;
    for (int i = header->lowest_block_address; i < SIZE; i += BLOCK_SIZE)
    {
        unsigned short block_address_header = *(unsigned short *) &data[i];
        if (block_address_header != 0)  // this is an allocated block, we next lowest block address
        {
            new_lowest_block_address = i;
            break;  // leave
        }
    }

    // highest block in memory was freed, set back to 2031 virtual address -- init value
    if (header->lowest_block_address == new_lowest_block_address)
    {
        header->lowest_block_address = SIZE - 1;
    }
    else
    {
        header->lowest_block_address = new_lowest_block_address;
    }
}

void set_next_highest_queue_address()
{
    Header * header = (Header *) &data[0];
    unsigned short new_highest_queue_address = header->highest_queue_address;
    for (int i = header->highest_queue_address - QUEUE_HEADER_SIZE; i >= QUEUE_HEADER_SIZE; i -= QUEUE_HEADER_SIZE)
    {
        unsigned short queue_address_header = *(unsigned short *) &data[i];
        if (queue_address_header != 0)  // this is an allocated queue, we next highest queue address
        {
            new_highest_queue_address = i;
            break;  // leave
        }
    }

    // lowest queue in memory was freed, set back to 0 virtual address -- init value
    if (header->highest_queue_address == new_highest_queue_address)
    {
        header->highest_queue_address = 0;
    }
    else
    {
        header->highest_queue_address = new_highest_queue_address;
    }
}

// Resize two blocks into one.
void resize_queue_blocks(Q * q)
{
    QNode * new_first_block = (QNode *) &data[q->last_block_address];
    QNode * current_block = new_first_block;
    for (int i = q->size-1; i >= 0; i--)
    {
        new_first_block->bytes[i] = current_block->bytes[q->last];
        q->last--;
        if (q->last == -1)
        {
            q->last = BLOCK_DATA_SIZE - 1; // wrap q->last to the end of the first block
            current_block = (QNode *) &data[q->first_block_address];
            current_block->next_block_address = 0;  // free this block in virtual memory
        }
    }

    Header * header = (Header *) &data[0];
    // set this to the highest free block address if thats the case
    if (q->first_block_address > header->highest_free_block_address)
    {
        header->highest_free_block_address = q->first_block_address;
    }

    // if it was also the lowest block address, step through higher virtual memory and find another one
    if (q->first_block_address == header->lowest_block_address)
    {
        set_next_lowest_block_address();
    }

    q->first_block_address = q->last_block_address; // replace the old first block
    q->first = 0;
    q->last = q->size-1;
}

// Allocates memory for a block of bytes.
unsigned short qn_malloc()
{
    Header * header = (Header *) &data[0];

    // if the highest free block address will pass through the required memory for the queue, there is no memory
    if (header->highest_free_block_address < header->highest_queue_address + QUEUE_HEADER_SIZE)
    {
        printf("qn_malloc: OUT OF MEMORY!!!\n");
        on_out_of_memory();
    }

    unsigned short block_address = header->highest_free_block_address;
    QNode * qnode = (QNode *) &data[block_address];
    qnode->next_block_address = 1;  // claim this block by marking the header


    if (header->highest_free_block_address < header->lowest_block_address)  // block allocations are contiguous
    {
        header->lowest_block_address = header->highest_free_block_address;
        header->highest_free_block_address = header->lowest_block_address - BLOCK_SIZE;
    }
    else
    {
        // search for the next highest free block address going lower in virtual memory
        unsigned short block_address_header = 0;
        for (int i = header->highest_free_block_address - BLOCK_SIZE; i >= header->lowest_block_address + QUEUE_HEADER_SIZE; i -= BLOCK_SIZE)
        {
            block_address_header = *(unsigned short *) &data[i];
            if (block_address_header == 0)
            {
                header->highest_free_block_address = i;
                break;  // found the highest free block address, leave
            }
        }

        // the next free block is below the lowest block address
        if (block_address_header != 0)
        {
            header->highest_free_block_address = header->lowest_block_address - BLOCK_SIZE;
        }
    }

    return block_address;
}

void init_data()
{
    Header * header = (Header *) &data[0];
    header->lowest_free_queue_address = MEMORY_HEADER_SIZE;
    header->highest_free_block_address = SIZE - BLOCK_SIZE;
    header->highest_queue_address = 0;
    header->lowest_block_address = SIZE - 1;
}

// Creates a FIFO byte queue, returning a handle to it.
Q * create_queue()
{
    Header * header = (Header *) &data[0];
    if (header->lowest_free_queue_address == 0)
    {
        init_data();
    }
    
    // if the highest free block address will pass through the required memory for the queue, there is no memory
    if (header->highest_free_block_address < header->highest_queue_address + QUEUE_HEADER_SIZE)
    {
        printf("create_queue: OUT OF MEMORY!!!\n");
        on_out_of_memory();
    }

    Q * q = (Q *) &data[header->lowest_free_queue_address];
    q->first_block_address = qn_malloc();
    q->last_block_address = q->first_block_address;
    q->size = 0;
    q->first = 0;
    q->last = BLOCK_DATA_SIZE - 1;

    if (header->lowest_free_queue_address > header->highest_queue_address)  // queue allocations are contiguous
    {
        header->highest_queue_address = header->lowest_free_queue_address;
        header->lowest_free_queue_address = header->highest_queue_address + QUEUE_HEADER_SIZE;
    }
    else
    {
        // search for the next lowest free queue address going higher in virtual memory
        unsigned short queue_address_header = 0;
        for (int i = header->lowest_free_queue_address; i <= header->highest_queue_address; i += QUEUE_HEADER_SIZE)
        {
            queue_address_header = *(unsigned short *) &data[i];
            if (queue_address_header == 0)
            {
                header->lowest_free_queue_address = i;
                break;  // found the lowest free queue address, leave
            }
        }

        // the next free queue is above the highest queue address
        if (queue_address_header != 0)
        {
            header->lowest_free_queue_address = header->highest_queue_address + QUEUE_HEADER_SIZE;
        }
    }

    return q;
}

void destroy_block(unsigned short address)
{
    if (address == 0) return;
    if (address == 1) return;   // recursive base case

    QNode * qnode = (QNode *) &data[address];
    unsigned short next_block_address = qnode->next_block_address;
    qnode->next_block_address = 0;

    destroy_block(next_block_address);

    Header * header = (Header *) &data[0];
    // we have to check each time for these two conditions
    // any block of data in the queue can be the available highest free block address
    if (address > header->highest_free_block_address)
    {
        header->highest_free_block_address = address;
    }
    // any block of data in the queue might be the lowest block address
    if (address == header->lowest_block_address)
    {
        set_next_lowest_block_address();
    }
}

// Destroy an earlier created byte queue.
void destroy_queue(Q * q)
{
    if (q == NULL)
    {
        on_illegal_operation();
    }
    if (q->first_block_address == 0)
    {
        printf("destroy_queue: CANNOT DESTROY DEREFERENCED QUEUE!\n");
        on_illegal_operation();
    }

    Header * header = (Header *) &data[0];
    destroy_block(q->first_block_address);  // recursively destroy blocks

    // find the virtual memory address for the queue being destroyed
    for (int i = MEMORY_HEADER_SIZE; i <= header->highest_queue_address; i += QUEUE_HEADER_SIZE)
    {
        Q * compare_q = (Q *) &data[i];
        if (q->first_block_address == compare_q->first_block_address)   // found it
        {
            // he destroyed queue address qualifies to be the highest free queue address
            if (i < header->lowest_free_queue_address) header->lowest_free_queue_address = i;
            // if this was highest queue address, step through lower virtual memory to find the next allocated queue
            if (i == header->highest_queue_address) set_next_highest_queue_address();
            break;  // leave
        }
    }

    q->first_block_address = 0; // reset this queue header to free
}

// Adds a new byte to a queue.
void enqueue_byte(Q * q, unsigned char b)
{
    if (q == NULL)
    {
        on_illegal_operation();
    }
    if (q->first_block_address == 0)
    {
        on_illegal_operation();
    }

    QNode * qnode = (QNode *) &data[q->last_block_address];

    if (q->last == BLOCK_DATA_SIZE - 1 && q->size >= BLOCK_DATA_SIZE) // we're on the last byte of a block
    {
        q->last_block_address = qn_malloc();    // alloc memory for the next block
        qnode->next_block_address = q->last_block_address;  // reference the new block to the old last block
        qnode = (QNode *) &data[q->last_block_address];
        q->last = 0;    // reset to the beginning of this block
    }
    else
    {
        q->last = (q->last + 1) % BLOCK_DATA_SIZE;
    }

    qnode->bytes[q->last] = b;
    q->size++;
}

// Pops the next byte off the FIFO queue
unsigned char dequeue_byte(Q * q)
{
    if (q == NULL)
    {
        on_illegal_operation();
    }
    if (q->first_block_address == 0)
    {
        on_illegal_operation();
    }

    if (q->size == 0)
    {
        printf("dequeue_byte: NOTHING LEFT TO DEQUEUE!\n");
        on_illegal_operation();
    }

    unsigned char byte = 0;

    QNode * qnode = (QNode *) &data[q->first_block_address];

    byte = qnode->bytes[q->first];  // the byte is stored for retrieval now
    qnode->bytes[q->first] = 0;
    q->size--;

    // shift blocks over if too much space is given for a small queue size between two blocks
    // example diagram:
    // q->first = 11
    // q->last  = 2
    // [ 0 0 0 0 0 0 0 0 0 0 + + + + ] [ + + + 0 0 0 0 0 0 0 0 0 0 0 ]
    // [ x x x x x x x x x x x x x x ] [ + + + + + + 0 0 0 0 0 0 0 0 ]
    unsigned char resize_blocks = q->first_block_address != q->last_block_address && q->size < BLOCK_DATA_SIZE / 2;
    if (resize_blocks)
    {
        resize_queue_blocks(q);
    }
    else if (q->first == BLOCK_DATA_SIZE - 1 && q->size >= BLOCK_DATA_SIZE)  // last byte in first block
    {
        Header * header = (Header *) &data[0];
        unsigned short original_first_block_address = q->first_block_address;

        q->first = 0;   // set first to the beginning of a block (for the next block)
        q->first_block_address = qnode->next_block_address; // make the next block the first block
        qnode->next_block_address = 0;  // free this block to be picked up by qn_malloc

        // if the original first block address was also the lowest block address,
        // step through higher virtual memory and find another one
        if (original_first_block_address == header->lowest_block_address)
        {
            set_next_lowest_block_address();
        }
        
        // the original first block address qualifies to be the highest free block address
        if (original_first_block_address > header->highest_free_block_address)
        {
            header->highest_free_block_address = original_first_block_address;
        }
    }
    else
    {
        q->first = (q->first + 1) % BLOCK_DATA_SIZE;
    }

    return byte;
}

void printQueue(Q * q)
{
    if (q == NULL) return;
    if (q->first_block_address == 0) return;

    printf("{\n  first_block_address: %d\n  final_block_address: %d\n  size: %d\n  first: %d\n  last: %d\n}\n", 
        q->first_block_address, q->last_block_address, q->size, q->first, q->last);

    unsigned short block_address = q->first_block_address;
    int block_count = 1;
    while (block_address > 3)
    {
        printf("block [%d]\n", block_count);
        QNode * qnode = (QNode *) &data[block_address];
        printf("{\n  next_block_address: %d\n", qnode->next_block_address);
        printf("  [\n    ");
        for (int i = 0; i < BLOCK_DATA_SIZE; i++)
        {
            printf(" %x ", qnode->bytes[i]);
        }
        printf("\n  ]\n}\n");

        block_address = qnode->next_block_address;
        block_count++;
    }
}

// header->lowest_free_queue_address = MEMORY_HEADER_SIZE; 8
// header->highest_free_block_address = SIZE - BLOCK_SIZE; 2032
// header->highest_queue_address = 0;
// header->lowest_block_address = SIZE; 2048
void printHeader()
{
    Header * header = (Header *) &data[0];
    printf("HEADER\n+++\n{\n  lowest_free_queue_address: %d\n  highest_free_block_address: %d\n  highest_queue_address: %d\n  lowest_block_address: %d\n}\n",
        header->lowest_free_queue_address, header->highest_free_block_address,
        header->highest_queue_address, header->lowest_block_address
    );
}

int main() {
    Q * q0 = create_queue();
    enqueue_byte(q0, 0);
    enqueue_byte(q0, 1);
    Q * q1 = create_queue();
    enqueue_byte(q1, 3);
    enqueue_byte(q0, 2);
    enqueue_byte(q1, 4);
    printf("%d", dequeue_byte(q0));
    printf("%d\n", dequeue_byte(q0));
    enqueue_byte(q0, 5);
    enqueue_byte(q1, 6);
    printf("%d", dequeue_byte(q0));
    printf("%d\n", dequeue_byte(q0));
    destroy_queue(q0);
    printf("%d", dequeue_byte(q1));
    printf("%d", dequeue_byte(q1));
    printf("%d\n", dequeue_byte(q1));
    destroy_queue(q1);

    return 0;
}