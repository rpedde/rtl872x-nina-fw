#ifndef __HANDLERS_H__
#define __HANDLERS_H__

typedef int (*command_handler)(const uint8_t *, uint8_t *);

extern command_handler handler_for(uint8_t);

#endif /*__HANDLERS_H__ */
