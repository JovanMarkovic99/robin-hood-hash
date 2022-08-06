# robin-hood-hash-map

A hashmap implementation, using the RobinHood algorithm.\
https://dspace.mit.edu/bitstream/handle/1721.1/130693/1251799942-MIT.pdf \
\
By default, it should be noticeably faster than std::unordered_map, although further optimization is necessary.

## Memory layout

The memory layout consists of a continuous array (bucket) of special elements.
The special element is one information byte, containing the distance from the hash position,
followed by a key-value pair. Special elements aren't constructed until a key-value is inserted.

The size of the bucket is always 2^n in order to optimally trim the hash.
