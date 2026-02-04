#ifndef PROVISIONING_MANAGER_H
#define PROVISIONING_MANAGER_H

#include <stdbool.h>

void provisioning_start(void);
void provisioning_stop(void);

bool provisioning_is_done(void);
bool provisioning_has_failed(void);

#endif
