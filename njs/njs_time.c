
/*
 * Copyright (C) Dmitry Volyntsev
 * Copyright (C) NGINX, Inc.
 */

#include <njs_core.h>
#include <njs_time.h>
#include <string.h>
#include <stdio.h>


static njs_ret_t
njs_set_timer(njs_vm_t *vm, njs_value_t *args, nxt_uint_t nargs,
    njs_index_t unused, nxt_bool_t immediate)
{
    nxt_uint_t     n;
    uint64_t       delay;
    njs_event_t   *event;
    njs_vm_ops_t  *ops;

    if (nxt_slow_path(nargs < 2)) {
        njs_type_error(vm, "too few arguments");
        return NJS_ERROR;
    }

    if (nxt_slow_path(!njs_is_function(&args[1]))) {
        njs_type_error(vm, "first arg must be a function");
        return NJS_ERROR;
    }

    ops = vm->options.ops;
    if (nxt_slow_path(ops == NULL)) {
        njs_internal_error(vm, "not supported by host environment");
        return NJS_ERROR;
    }

    delay = 0;

    if (!immediate && nargs >= 3 && njs_is_number(&args[2])) {
        delay = args[2].data.u.number;
    }

    event = nxt_mp_alloc(vm->mem_pool, sizeof(njs_event_t));
    if (nxt_slow_path(event == NULL)) {
        goto memory_error;
    }

    n = immediate ? 2 : 3;

    event->destructor = ops->clear_timer;
    event->function = args[1].data.u.function;
    event->nargs = (nargs >= n) ? nargs - n : 0;
    event->once = 1;
    event->posted = 0;

    if (event->nargs != 0) {
        event->args = nxt_mp_alloc(vm->mem_pool,
                                   sizeof(njs_value_t) * event->nargs);
        if (nxt_slow_path(event->args == NULL)) {
            goto memory_error;
        }

        memcpy(event->args, &args[n], sizeof(njs_value_t) * event->nargs);
    }

    event->host_event = ops->set_timer(vm->external, delay, event);
    if (nxt_slow_path(event->host_event == NULL)) {
        njs_internal_error(vm, "set_timer() failed");
        return NJS_ERROR;
    }

    return njs_add_event(vm, event);

memory_error:

    njs_memory_error(vm);

    return NJS_ERROR;
}


njs_ret_t
njs_set_timeout(njs_vm_t *vm, njs_value_t *args, nxt_uint_t nargs,
    njs_index_t unused)
{
    return njs_set_timer(vm, args, nargs, unused, 0);
}


njs_ret_t
njs_set_immediate(njs_vm_t *vm, njs_value_t *args, nxt_uint_t nargs,
    njs_index_t unused)
{
    return njs_set_timer(vm, args, nargs, unused, 1);
}


njs_ret_t
njs_clear_timeout(njs_vm_t *vm, njs_value_t *args, nxt_uint_t nargs,
    njs_index_t unused)
{
    u_char              buf[16], *p;
    njs_ret_t           ret;
    njs_event_t         *event;
    nxt_lvlhsh_query_t  lhq;

    if (nxt_fast_path(nargs < 2) || !njs_is_number(&args[1])) {
        vm->retval = njs_value_void;
        return NJS_OK;
    }

    p = nxt_sprintf(buf, buf + nxt_length(buf), "%uD",
                    (unsigned) args[1].data.u.number);

    lhq.key.start = buf;
    lhq.key.length = p - buf;
    lhq.key_hash = nxt_djb_hash(lhq.key.start, lhq.key.length);
    lhq.proto = &njs_event_hash_proto;
    lhq.pool = vm->mem_pool;

    ret = nxt_lvlhsh_find(&vm->events_hash, &lhq);
    if (ret == NXT_OK) {
        event = lhq.value;
        njs_del_event(vm, event, NJS_EVENT_RELEASE | NJS_EVENT_DELETE);
    }

    vm->retval = njs_value_void;

    return NJS_OK;
}


const njs_object_init_t  njs_set_timeout_function_init = {
    nxt_string("setTimeout"),
    NULL,
    0,
};

const njs_object_init_t  njs_set_immediate_function_init = {
    nxt_string("setImmediate"),
    NULL,
    0,
};


const njs_object_init_t  njs_clear_timeout_function_init = {
    nxt_string("clearTimeout"),
    NULL,
    0,
};
