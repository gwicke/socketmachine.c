# socketmachine.c
Old fun project trying to implement sync-looking async code using C macros and computed gotos

See [the SM_CALL implementation][call] and [how it is used][use]:

```c
if(SM_CALL(evaio_recv_peek, fd, s->buf+100, 1000-103, childco) < 0) {
  ...
}
```

[call]: https://github.com/gwicke/socketmachine.c/blob/master/socketmachine.h#L185-L205
[use]: https://github.com/gwicke/socketmachine.c/blob/master/socketmachine.c#L170
