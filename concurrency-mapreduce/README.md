
# Map Reduce

In 2004, engineers at Google introduced a new paradigm for large-scale
parallel data processing known as MapReduce (see the original paper
[here](https://static.googleusercontent.com/media/research.google.com/en//archive/mapreduce-osdi04.pdf),
and make sure to look in the citations at the end). One key aspect of
MapReduce is that it makes programming such tasks on large-scale clusters easy
for developers; instead of worrying about how to manage parallelism, handle
machine crashes, and many other complexities common within clusters of
machines, the developer can instead just focus on writing little bits of code
(described below) and the infrastructure handles the rest.

In this project, you'll be building a simplified version of MapReduce for just
a single machine. While somewhat easier than with a single machine, there are
still numerous challenges, mostly in building the correct concurrency
support. Thus, you'll have to think a bit about how to build the MapReduce
implementation, and then build it work efficiently and correctly.

There are three specific objectives to this assignment:

- To learn bout the general nature of the MapReduce paradigm.
- To implement a correct and efficient MapReduce framework using threads and
  related functions.
- To gain more experience writing concurrent code.


## Background

To understand how to make progress on this project, you should understand the
basics of thread creation, mutual exclusion (with locks), and
signaling/waiting (with condition variables). These are described in the
following book chapters:

- [Intro to Threads](http://pages.cs.wisc.edu/~remzi/OSTEP/threads-intro.pdf)
- [Threads API](http://pages.cs.wisc.edu/~remzi/OSTEP/threads-api.pdf)
- [Locks](http://pages.cs.wisc.edu/~remzi/OSTEP/threads-locks.pdf)
- [Using Locks](http://pages.cs.wisc.edu/~remzi/OSTEP/threads-locks-usage.pdf)
- [Condition Variables](http://pages.cs.wisc.edu/~remzi/OSTEP/threads-cv.pdf)

Read these chapters carefully in order to prepare yourself for this project.

## General Idea

Let's now get into the exact code you'll have to build. The MapReduce
infrastructure you will build supports the execution of user-defined `Map()`
and `Reduce()` functions.

As from the original paper: "`Map()`, written by the user, takes an input pair
and produces a set of intermediate key/value pairs. The MapReduce library
groups together all intermediate values associated with the same intermediate
key K and passes them to the `Reduce()` function."

"The `Reduce()` function, also written by the user, accepts an intermediate
key K and a set of values for that key. It merges together these values to
form a possibly smaller set of values; typically just zero or one output value
is produced per `Reduce()` invocation. The intermediate values are supplied to
the user's reduce function via an iterator."

A classic example, written here in pseudocode, shows how to count the number
of occurrences of each word in a set of documents:

```
map(String key, String value):
  // key: document name
  // value: document contents
  for each word w in value:
    EmitIntermediate(w, "1");

reduce(String key, Iterator values):
  // key: a word
  // values: a list of counts
  int result = 0;
  for each v in values:
    result += ParseInt(v);
  print key, result;
```

What's fascinating about MapReduce is that so many different kinds of relevant
computations can be mapped onto this framework. The original paper lists many
examples, including word counting (as above), a distributed grep, a URL
frequency access counters, a reverse web-link graph application, a term-vector
per host analysis, and others. 

What's also quite interesting is how easy it is to parallelize: many mappers
can be running at the same time, and later, many reducers can be running at
the same time. Users don't have to worry about how to parallelize their
application; rather, they just write `Map()` and `Reduce()` functions and the
infrastructure does the rest.

## Details

We give you here `mapreduce.h` file that specifies exactly what you must build
in your MapReduce library:

```
#ifndef __mapreduce_h__
#define __mapreduce_h__

// Various function pointers
typedef char *(*Getter)();
typedef void (*Mapper)(char *file_name);
typedef void (*Reducer)(char *key, Getter get_func, int partition_number);
typedef unsigned long (*Partitioner)(char *key, int num_buckets);

// Key functions exported by MapReduce
void MR_Emit(char *key, char *value);
unsigned long MR_DefaultHashPartition(char *key, int num_buckets);
void MR_Run(int argc, char *argv[],
            Mapper map, int num_mappers,
            Reducer reduce, int num_reducers,
            Partitioner partition);
```

The most important function is `MR_Run`, which takes the command line
parameters of a given program, a pointer to a Map function (type `Mapper`,
called `map`), the number of mapper threads your library should create
(`num_mappers`), a pointer to a Reduce function (type `Reducer`, called
`reduce`), the number of reducers (`num_reducers`), and finally, a pointer to
a Partition function (`partition`, described below).

Thus, when a user is writing a MapReduce computation with your library, they
will implement a Map function, implement a Reduce function, possibly implement
a Partition function, and then call `MR_Run()`. The infrastructure will then
create threads as appropriate and run the computation.

Here is a simple (but functional) wordcount program, written to use this
infrastructure: 

```
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "mapreduce.h"

void Map(char *file_name) {
    FILE *fp = fopen(file_name, "r");
    assert(fp != NULL);

    char *line = NULL;
    size_t size = 0;
    while (getline(&line, &size, fp) != -1) {
        char *token, *dummy = line;
        while ((token = strsep(&dummy, " \t\n\r")) != NULL) {
            MR_Emit(token, "1");
        }
    }

    fclose(fp);
}

void Reduce(char *key, Getter get_next, int partition_number) {
    int count = 0;
    char *value;
    while ((value = get_next(partition_number)) != NULL)
        count++;
    printf("%s %d\n", key, count);
}

int main(int argc, char *argv[]) {
    MR_Run(argc, argv, Map, 10, Reduce, 10, MR_DefaultHashPartition);
}
```

Let's walk through this code, in order to see what it is doing. First, notice
that `Map()` is called with a file name. In general, we assume that this type
of computation is being run over many files; each invocation of `Map()` is
thus handed one file name and is expected to process that file in its
entirety. 

In this example, the code above just reads through the file, one line at a
time, and uses `strsep()` to chop the line into tokens. Each token is then
emitted using the `MR_Emit()` function, which takes two strings as input: a
key and a value. The key here is the word itself, and the token is just a
count, in this case, 1 (as a string). It then closes the file.

The `MR_Emit()` function is thus another key part of your library; it needs to
take key/value pairs from the many different mappers and store them in a way 
that later reducers can access them, given constraints described
below. Designing and implementing this data structure is thus a central
challenge of the project.

After the mappers are finished, your library should have stored the key/value
pairs in such a way that the `Reduce()` function can be called. `Reduce()` is
invoked once per key, and is passed the key along with a function that enables
iteration over all of the values that produced that same key. To iterate, the
code just calls `get_next()` repeatedly until a NULL value is returned;
`get_next` returns a pointer to the value passed in by the `MR_Emit()`
function above. 








## Considerations


- **Memory Management**. yyy.



## Grading

Your code should turn in `mapreduce.c` which implements the above functions
correctly and efficiently. It will be compiled with test applications with the
`-Wall -Werror -pthread -O` flags; it will also be valgrinded to check for
memory errors.

Your code will first be measured for correctness, ensuring that it performs
the maps and reductions correctly. If you pass the correctness tests, your
code will be tested for performance; higher performance will lead to better
scores.



