/* raft89_config.c - pure configuration validation. */
#include <stddef.h>

#include "raft89_internal.h"

int raft89__config_has_zero(const raft89_config *config)
{
    raft89_size i;
    for (i = 0u; i < config->member_count; ++i)
    {
        if (config->members[i] == RAFT89_ID_NONE)
        {
            return 1;
        }
    }
    return 0;
}

int raft89__config_has_duplicate(const raft89_config *config)
{
    raft89_size i;
    raft89_size j;
    for (i = 0u; i < config->member_count; ++i)
    {
        for (j = i + 1u; j < config->member_count; ++j)
        {
            if (config->members[i] == config->members[j])
            {
                return 1;
            }
        }
    }
    return 0;
}

unsigned long raft89__config_count_id(const raft89_config *config, raft89_id id)
{
    unsigned long count;
    raft89_size i;
    count = 0u;
    for (i = 0u; i < config->member_count; ++i)
    {
        if (config->members[i] == id)
        {
            ++count;
        }
    }
    return count;
}

int raft89__members_valid(const raft89_config *config)
{
    if (raft89__config_has_zero(config) != 0)
    {
        return RAFT89_ERR_ARG;
    }
    if (raft89__config_has_duplicate(config) != 0)
    {
        return RAFT89_ERR_ARG;
    }
    if (raft89__config_count_id(config, config->self) != 1u)
    {
        return RAFT89_ERR_ARG;
    }
    return RAFT89_OK;
}

int raft89__timeouts_valid(const raft89_config *config)
{
    if (config->heartbeat_interval == 0u)
    {
        return RAFT89_ERR_ARG;
    }
    if (config->election_timeout_min <= config->heartbeat_interval)
    {
        return RAFT89_ERR_ARG;
    }
    if (config->election_timeout_max < config->election_timeout_min)
    {
        return RAFT89_ERR_ARG;
    }
    return RAFT89_OK;
}

int raft89__limits_valid(const raft89_config *config)
{
    if (config->max_append_entries == 0u)
    {
        return RAFT89_ERR_ARG;
    }
    if (config->max_append_bytes == 0u)
    {
        return RAFT89_ERR_ARG;
    }
    return RAFT89_OK;
}

int raft89__store_valid(const raft89_config *config)
{
    if (config->store.hard_state == NULL)
    {
        return RAFT89_ERR_ARG;
    }
    if (config->store.log_last == NULL)
    {
        return RAFT89_ERR_ARG;
    }
    if (config->store.log_term == NULL)
    {
        return RAFT89_ERR_ARG;
    }
    if (config->store.log_size == NULL)
    {
        return RAFT89_ERR_ARG;
    }
    if (config->store.log_read == NULL)
    {
        return RAFT89_ERR_ARG;
    }
    return RAFT89_OK;
}

int raft89__random_valid(const raft89_config *config)
{
    if (config->random.next == NULL)
    {
        return RAFT89_ERR_ARG;
    }
    return RAFT89_OK;
}

int raft89__config_validate(const raft89_config *config)
{
    int rc;
    if (config == NULL)
    {
        return RAFT89_ERR_ARG;
    }
    if (config->members == NULL)
    {
        return RAFT89_ERR_ARG;
    }
    if (config->member_count == 0u)
    {
        return RAFT89_ERR_ARG;
    }
    if (config->self == RAFT89_ID_NONE)
    {
        return RAFT89_ERR_ARG;
    }
    rc = raft89__members_valid(config);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    rc = raft89__timeouts_valid(config);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    rc = raft89__limits_valid(config);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    rc = raft89__store_valid(config);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    return raft89__random_valid(config);
}
