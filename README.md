


### GCC如果编译没有指定优化，内联函数不会内联
[gcc文档](https://gcc.gnu.org/onlinedocs/gcc/Inline.html): GCC does not inline any functions when not optimizing unless you specify the ‘always_inline’ attribute for the function, like this:


