# socketmachine.c
Old (~2007) fun project implementing sync-looking stackless async code using continuation passing. The implementation (ab)uses (GC)C macros and computed gotos, in the spirit of [Duff's device][duff].

See [the SM_CALL implementation][call] and [how it is used][use]:

```c
if(SM_CALL(evaio_recv_peek, fd, s->buf+100, 1000-103, childco) < 0) {
  ...
}
```

[call]: https://github.com/gwicke/socketmachine.c/blob/master/socketmachine.h#L185-L205
[use]: https://github.com/gwicke/socketmachine.c/blob/master/socketmachine.c#L170
[duff]: https://en.wikipedia.org/wiki/Duff%27s_device
