/**
* Copyright (c) NVIDIA CORPORATION & AFFILIATES, 2001-2019. ALL RIGHTS RESERVED.
* Copyright (C) UT-Battelle, LLC. 2014. ALL RIGHTS RESERVED.
* See file LICENSE for terms.
*/
/* force older C++ version to have SIZE_MAX */
#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#include <common/test.h>
extern "C" {
#include <ucs/config/parser.h>
#include <ucs/time/time.h>
}

#define TEST_CONFIG_DIR  TOP_SRCDIR "/test/gtest/ucs"
#define TEST_CONFIG_FILE "ucx_test.conf"
#define TEST_ENV_PREFIX  "UCXGTEST_"


typedef enum {
    COLOR_RED,
    COLOR_BLUE,
    COLOR_BLACK,
    COLOR_YELLOW,
    COLOR_WHITE,
    COLOR_LAST
} color_t;

typedef enum {
    MATERIAL_LEATHER,
    MATERIAL_ALCANTARA,
    MATERIAL_TEXTILE,
    MATERIAL_LAST
} material_t;

const char *color_names[] = {
    /* [COLOR_RED]    = */ "red",
    /* [COLOR_BLUE]   = */ "blue",
    /* [COLOR_BLACK]  = */ "black",
    /* [COLOR_YELLOW] = */ "yellow",
    /* [COLOR_WHITE]  = */ "white",
    /* [COLOR_LAST]   = */ NULL
};

const char *material_names[] = {
    /* [MATERIAL_LEATHER]   = */ "leather",
    /* [MATERIAL_ALCANTARA] = */ "alcantara",
    /* [MATERIAL_TEXTILE]   = */ "textile",
    /* [MATERIAL_LAST]      = */ NULL
};

typedef struct {
    color_t         color;
    material_t      material;
} seat_opts_t;

typedef struct {
    seat_opts_t     driver_seat;
    seat_opts_t     passenger_seat;
    seat_opts_t     rear_seat;
} coach_opts_t;

typedef struct {
    const char      *model;
    unsigned        volume;
    unsigned long   power;
} engine_opts_t;

typedef struct {
    engine_opts_t   engine;
    coach_opts_t    coach;
    unsigned        price;
    const char      *brand;
    const char      *model;
    color_t         color;
    unsigned long   vin;

    double          bw_bytes;
    double          bw_kbytes;
    double          bw_mbytes;
    double          bw_gbytes;
    double          bw_tbytes;
    double          bw_bits;
    double          bw_kbits;
    double          bw_mbits;
    double          bw_gbits;
    double          bw_tbits;
    double          bw_auto;

    ucs_config_bw_spec_t can_pci_bw; /* CAN-bus */

    int             air_conditioning;
    int             abs;
    int             transmission;

    ucs_time_t      time_value;
    ucs_time_t      time_auto;
    ucs_time_t      time_inf;
    ucs_config_allow_list_t allow_list;

    int             temp_front;
    int             temp_rear;

    const char      *passengers[3];
} car_opts_t;


ucs_config_field_t seat_opts_table[] = {
  {"COLOR", "black", "Seat color",
   ucs_offsetof(seat_opts_t, color), UCS_CONFIG_TYPE_ENUM(color_names)},

  {"COLOR_ALIAS", NULL, "Seat color",
   ucs_offsetof(seat_opts_t, color), UCS_CONFIG_TYPE_ENUM(color_names)},

  {"MATERIAL", "textile", "Cover seat material",
   ucs_offsetof(seat_opts_t, material), UCS_CONFIG_TYPE_ENUM(material_names)},

  {NULL}
};

ucs_config_field_t coach_opts_table[] = {
  {"DRIVER_", "COLOR=red", "Driver seat options",
   ucs_offsetof(coach_opts_t, driver_seat), UCS_CONFIG_TYPE_TABLE(seat_opts_table)},

  {"PASSENGER_", "", "Passenger seat options",
   ucs_offsetof(coach_opts_t, passenger_seat), UCS_CONFIG_TYPE_TABLE(seat_opts_table)},

  {"REAR_", "", "Rear seat options",
   ucs_offsetof(coach_opts_t, rear_seat), UCS_CONFIG_TYPE_TABLE(seat_opts_table)},

  {NULL}
};

ucs_config_field_t engine_opts_table[] = {
  {"MODEL", "auto", "Engine model",
   ucs_offsetof(engine_opts_t, model), UCS_CONFIG_TYPE_STRING},

  {"VOLUME", "6000", "Engine volume",
   ucs_offsetof(engine_opts_t, volume), UCS_CONFIG_TYPE_UINT},

  {"POWER", "200", "Engine power",
   ucs_offsetof(engine_opts_t, power), UCS_CONFIG_TYPE_ULUNITS},

  {"POWER_ALIAS", NULL, "Engine power",
   ucs_offsetof(engine_opts_t, power), UCS_CONFIG_TYPE_ULUNITS},

  {"FUEL_LEVEL", "", "This is electric car",
   UCS_CONFIG_DEPRECATED_FIELD_OFFSET, UCS_CONFIG_TYPE_DEPRECATED},

  {NULL}
};

ucs_config_field_t car_opts_table[] = {
  {"ENGINE_", "", "Engine options",
   ucs_offsetof(car_opts_t, engine), UCS_CONFIG_TYPE_TABLE(engine_opts_table)},

  {"COACH_", "PASSENGER_COLOR=blue", "Seats options",
   ucs_offsetof(car_opts_t, coach), UCS_CONFIG_TYPE_TABLE(coach_opts_table)},

  {"PRICE", "999", "Price",
   ucs_offsetof(car_opts_t, price), UCS_CONFIG_TYPE_UINT},

  {"PRICE_ALIAS", NULL, "Price",
   ucs_offsetof(car_opts_t, price), UCS_CONFIG_TYPE_UINT},

  {"DRIVER", "", "AI drives a car",
   UCS_CONFIG_DEPRECATED_FIELD_OFFSET, UCS_CONFIG_TYPE_DEPRECATED},

  {"BRAND", "Chevy", "Car brand",
   ucs_offsetof(car_opts_t, brand), UCS_CONFIG_TYPE_STRING},

  {"MODEL", "Corvette", "Car model",
   ucs_offsetof(car_opts_t, model), UCS_CONFIG_TYPE_STRING},

  {"COLOR", "red", "Car color",
   ucs_offsetof(car_opts_t, color), UCS_CONFIG_TYPE_ENUM(color_names)},

  {"VIN", "auto", "Vehicle identification number",
   ucs_offsetof(car_opts_t, vin), UCS_CONFIG_TYPE_ULUNITS},

  {"BW_BYTES", "1024Bs", "Bandwidth in bytes",
   ucs_offsetof(car_opts_t, bw_bytes), UCS_CONFIG_TYPE_BW},

  {"BW_KBYTES", "1024KB/s", "Bandwidth in kbytes",
   ucs_offsetof(car_opts_t, bw_kbytes), UCS_CONFIG_TYPE_BW},

  {"BW_MBYTES", "1024MBs", "Bandwidth in mbytes",
   ucs_offsetof(car_opts_t, bw_mbytes), UCS_CONFIG_TYPE_BW},

  {"BW_GBYTES", "1024GBps", "Bandwidth in gbytes",
   ucs_offsetof(car_opts_t, bw_gbytes), UCS_CONFIG_TYPE_BW},

  {"BW_TBYTES", "1024TB/s", "Bandwidth in tbytes",
   ucs_offsetof(car_opts_t, bw_tbytes), UCS_CONFIG_TYPE_BW},

  {"BW_BITS", "1024bps", "Bandwidth in bits",
   ucs_offsetof(car_opts_t, bw_bits), UCS_CONFIG_TYPE_BW},

  {"BW_KBITS", "1024Kb/s", "Bandwidth in kbits",
   ucs_offsetof(car_opts_t, bw_kbits), UCS_CONFIG_TYPE_BW},

  {"BW_MBITS", "1024Mbs", "Bandwidth in mbits",
   ucs_offsetof(car_opts_t, bw_mbits), UCS_CONFIG_TYPE_BW},

  {"BW_GBITS", "1024Gbps", "Bandwidth in gbits",
   ucs_offsetof(car_opts_t, bw_gbits), UCS_CONFIG_TYPE_BW},

  {"BW_TBITS", "1024Tbs", "Bandwidth in tbits",
   ucs_offsetof(car_opts_t, bw_tbits), UCS_CONFIG_TYPE_BW},

  {"BW_AUTO", "auto", "Auto bandwidth value",
   ucs_offsetof(car_opts_t, bw_auto), UCS_CONFIG_TYPE_BW},

  {"CAN_BUS_BW", "mlx5_0:1024Tbs", "Bandwidth in tbits of CAN-bus",
   ucs_offsetof(car_opts_t, can_pci_bw), UCS_CONFIG_TYPE_BW_SPEC},

  {"AIR_CONDITIONING", "on", "Air conditioning mode",
   ucs_offsetof(car_opts_t, air_conditioning), UCS_CONFIG_TYPE_ON_OFF},

  {"ABS", "off", "ABS mode",
   ucs_offsetof(car_opts_t, abs), UCS_CONFIG_TYPE_ON_OFF},

  {"TRANSMISSION", "auto", "Transmission mode",
   ucs_offsetof(car_opts_t, transmission), UCS_CONFIG_TYPE_ON_OFF_AUTO},

  {"TIME_VAL", "1s", "Time value 1 sec",
   ucs_offsetof(car_opts_t, time_value), UCS_CONFIG_TYPE_TIME_UNITS},

  {"TIME_AUTO", "auto", "Time value \"auto\"",
   ucs_offsetof(car_opts_t, time_auto), UCS_CONFIG_TYPE_TIME_UNITS},

  {"TIME_INF", "inf", "Time value \"inf\"",
   ucs_offsetof(car_opts_t, time_inf), UCS_CONFIG_TYPE_TIME_UNITS},

  {"ALLOW_LIST", "all", "Allow-list: \"all\" OR \"val1,val2\" OR \"^val1,val2\"",
   ucs_offsetof(car_opts_t, allow_list), UCS_CONFIG_TYPE_ALLOW_LIST},

  {"TEMP", "20", "Temperature", 0,
    UCS_CONFIG_TYPE_KEY_VALUE(UCS_CONFIG_TYPE_UINT,
        {"front", "front temperature", ucs_offsetof(car_opts_t, temp_front)},
        {"rear",  "rear temperature",  ucs_offsetof(car_opts_t, temp_rear)},
        {NULL}
    )},

  {"PASSENGERS", "None", "Passengers", 0,
    UCS_CONFIG_TYPE_KEY_VALUE(UCS_CONFIG_TYPE_STRING,
        {"first",  "First passenger",  ucs_offsetof(car_opts_t, passengers[0])},
        {"second", "Second passenger", ucs_offsetof(car_opts_t, passengers[1])},
        {"third",  "Third passenger",  ucs_offsetof(car_opts_t, passengers[2])},
        {NULL}
    )},

  {NULL}
};

static std::vector<std::string> config_err_exp_str;

class test_config : public ucs::test {
public:
    test_config() {
        m_num_errors = 0;
    }

protected:
    static int m_num_errors;

    static ucs_log_func_rc_t
    config_error_handler(const char *file, unsigned line, const char *function,
                         ucs_log_level_t level,
                         const ucs_log_component_config_t *comp_conf,
                         const char *message, va_list ap)
    {
        // Ignore errors that invalid input parameters as it is expected
        if ((level == UCS_LOG_LEVEL_WARN) || (level == UCS_LOG_LEVEL_ERROR)) {
            std::string err_str = format_message(message, ap);

            for (size_t i = 0; i < config_err_exp_str.size(); i++) {
                if (err_str.find(config_err_exp_str[i]) != std::string::npos) {
                    UCS_TEST_MESSAGE << err_str;
                    return UCS_LOG_FUNC_RC_STOP;
                }
            }
        }

        return UCS_LOG_FUNC_RC_CONTINUE;
    }

    static ucs_log_func_rc_t
    config_error_suppress(const char *file, unsigned line, const char *function,
                          ucs_log_level_t level,
                          const ucs_log_component_config_t *comp_conf,
                          const char *message, va_list ap)
    {
        // Ignore errors that invalid input parameters as it is expected
        if (level == UCS_LOG_LEVEL_ERROR) {
            m_num_errors++;
            return wrap_errors_logger(file, line, function, level, comp_conf,
                                      message, ap);
        }

        return UCS_LOG_FUNC_RC_CONTINUE;
    }

    /*
     * Wrapper class for car options parser.
     */
    class car_opts {
    public:
        car_opts(const char *env_prefix, const char *table_prefix) :
            m_opts(parse(env_prefix, table_prefix)), m_max(1024), m_value(NULL)
        {
            m_value    = new char[m_max];
            m_value[0] = '\0';
        }

        car_opts(const car_opts& orig) : m_max(orig.m_max)
        {
            /* reset 'm_opts' to suppress Coverity warning that fields are not
             * initialized in the constructor */
            memset(&m_opts, 0, sizeof(m_opts));

            m_value = new char[m_max];
            strncpy(m_value, orig.m_value, m_max);

            ucs_status_t status = ucs_config_parser_clone_opts(&orig.m_opts,
                                                               &m_opts,
                                                               car_opts_table);
            ASSERT_UCS_OK(status);
        }

        ~car_opts() {
            ucs_config_parser_release_opts(&m_opts, car_opts_table);
            delete [] m_value;
        }

        ucs_status_t set(const char *name, const char *value)
        {
            return set(NULL, name, value);
        }

        ucs_status_t set(const char *prefix, const char *name,
                         const char *value)
        {
            return ucs_config_parser_set_value(&m_opts, car_opts_table, prefix,
                                               name, value);
        }

        const char* get(const char *name) {
            ucs_status_t status = ucs_config_parser_get_value(&m_opts,
                                                              car_opts_table,
                                                              name, m_value,
                                                              m_max);
            ASSERT_UCS_OK(status);
            return m_value;
        }

        car_opts_t* operator->() {
            return &m_opts;
        }

        car_opts_t* operator*() {
            return &m_opts;
        }

        std::string
        dump(ucs_config_print_flags_t flags, const char *filter = nullptr) const
        {
            char *dump_data = nullptr;
            size_t dump_size;
            char line_buf[1024];
            std::string res;

            FILE *file = open_memstream(&dump_data, &dump_size);
            ucs_config_parser_print_opts(file, "", &m_opts, car_opts_table,
                                         nullptr, UCS_DEFAULT_ENV_PREFIX, flags,
                                         filter);
            fseek(file, 0, SEEK_SET);

            while (fgets(line_buf, sizeof(line_buf), file)) {
                res += line_buf;
            }

            fclose(file);
            free(dump_data);
            return res;
        }

    private:

        static car_opts_t parse(const char *env_prefix,
                                const char *table_prefix) {
            /* coverity[tainted_string_argument] */
            ucs::scoped_setenv ucx_config_dir("UCX_CONFIG_DIR",
                                              TEST_CONFIG_DIR);
            car_opts_t tmp;
            ucs_status_t status;

            ucs_config_parse_config_files();

            static ucs_config_global_list_entry_t entry;
            entry.table  = car_opts_table;
            entry.name   = "cars";
            entry.prefix = table_prefix;
            entry.size   = sizeof(car_opts_t);
            entry.flags  = 0;

            status = ucs_config_parser_fill_opts(&tmp, &entry, env_prefix, 0);
            ASSERT_UCS_OK(status);
            return tmp;
        }

        car_opts_t   m_opts;
        const size_t m_max;
        char         *m_value;
    };

    static void test_config_print_opts(unsigned flags,
                                       unsigned exp_num_lines,
                                       const char *prefix = NULL)
    {
        char *dump_data;
        size_t dump_size;
        char line_buf[1024];
        char alias[128];
        car_opts opts(UCS_DEFAULT_ENV_PREFIX, NULL);

        memset(alias, 0, sizeof(alias));

        /* Dump configuration to a memory buffer */
        dump_data = NULL;
        FILE *file = open_memstream(&dump_data, &dump_size);
        ucs_config_parser_print_opts(file, "", *opts, car_opts_table, prefix,
                                     UCS_DEFAULT_ENV_PREFIX,
                                     (ucs_config_print_flags_t)flags, nullptr);

        /* Sanity check - all lines begin with UCS_ */
        unsigned num_lines = 0;
        fseek(file, 0, SEEK_SET);
        while (fgets(line_buf, sizeof(line_buf), file)) {
            if (line_buf[0] == '\n') {
                continue;
            }

            if (line_buf[0] != '#') {
                /* found the name of attribute */

                if (alias[0] != '\0') {
                    /* the code below relies on the fact that all
                     * aliases has the name: "<real_name>_ALIAS" */
                    EXPECT_EQ(0, strncmp(alias, line_buf,
                                         strlen(alias) - strlen("_ALIAS")));
                    memset(alias, 0, sizeof(alias));
                }

                std::string exp_str = "UCX_";
                if (prefix) {
                    exp_str += prefix;
                }
                line_buf[exp_str.size()] = '\0';
                EXPECT_STREQ(exp_str.c_str(), line_buf);
                ++num_lines;
            } else if (strncmp(&line_buf[2], "alias of:",
                               strlen("alias of:")) == 0) {
                /* found the alias name of attribute */

                size_t cnt = 0;
                for (size_t i = 2 + strlen("alias of: ") + 1;
                     line_buf[i] != '\n'; i++) {
                    alias[cnt++] = line_buf[i];
                }
            }
        }

        EXPECT_EQ(exp_num_lines, num_lines);

        fclose(file);
        free(dump_data);
    }
};

int test_config::m_num_errors;

UCS_TEST_F(test_config, parse_default) {
    car_opts opts(UCS_DEFAULT_ENV_PREFIX, "TEST");

    EXPECT_EQ(999U, opts->price);
    EXPECT_EQ(std::string("Chevy"), opts->brand);
    EXPECT_EQ(std::string("Corvette"), opts->model);
    EXPECT_EQ(COLOR_RED, opts->color);
    EXPECT_EQ(6000U, opts->engine.volume);
    EXPECT_EQ(COLOR_RED, opts->coach.driver_seat.color);
    EXPECT_EQ(COLOR_BLUE, opts->coach.passenger_seat.color);
    EXPECT_EQ(COLOR_BLACK, opts->coach.rear_seat.color);
    EXPECT_EQ(UCS_ULUNITS_AUTO, opts->vin);
    EXPECT_EQ(200UL, opts->engine.power);

    EXPECT_EQ(1024.0, opts->bw_bytes);
    EXPECT_EQ(UCS_KBYTE * 1024.0, opts->bw_kbytes);
    EXPECT_EQ(UCS_MBYTE * 1024.0, opts->bw_mbytes);
    EXPECT_EQ(UCS_GBYTE * 1024.0, opts->bw_gbytes);
    EXPECT_EQ(UCS_TBYTE * 1024.0, opts->bw_tbytes);

    EXPECT_EQ(128.0, opts->bw_bits);
    EXPECT_EQ(UCS_KBYTE * 128.0, opts->bw_kbits);
    EXPECT_EQ(UCS_MBYTE * 128.0, opts->bw_mbits);
    EXPECT_EQ(UCS_GBYTE * 128.0, opts->bw_gbits);
    EXPECT_EQ(UCS_TBYTE * 128.0, opts->bw_tbits);
    EXPECT_TRUE(UCS_CONFIG_DBL_IS_AUTO(opts->bw_auto));

    EXPECT_EQ(UCS_TBYTE * 128.0, opts->can_pci_bw.bw);
    EXPECT_EQ(std::string("mlx5_0"), opts->can_pci_bw.name);

    EXPECT_EQ(UCS_CONFIG_ON, opts->air_conditioning);
    EXPECT_EQ(UCS_CONFIG_OFF, opts->abs);
    EXPECT_EQ(UCS_CONFIG_AUTO, opts->transmission);

    EXPECT_EQ(ucs_time_from_sec(1.0), opts->time_value);
    EXPECT_EQ(UCS_TIME_AUTO, opts->time_auto);
    EXPECT_EQ(UCS_TIME_INFINITY, opts->time_inf);
    EXPECT_EQ(UCS_CONFIG_ALLOW_LIST_ALLOW_ALL, opts->allow_list.mode);
    EXPECT_EQ(0, opts->allow_list.array.count);

    EXPECT_EQ(20, opts->temp_front);
    EXPECT_EQ(20, opts->temp_rear);

    EXPECT_STREQ("None", opts->passengers[0]);
    EXPECT_STREQ("None", opts->passengers[1]);
    EXPECT_STREQ("None", opts->passengers[2]);
}

UCS_TEST_F(test_config, clone) {

    car_opts *opts_clone_ptr;

    {
        /* coverity[tainted_string_argument] */
        ucs::scoped_setenv env1("UCX_COLOR", "white");
        /* coverity[tainted_string_argument] */
        ucs::scoped_setenv env2("UCX_PRICE_ALIAS", "0");
        /* coverity[tainted_string_argument] */
        ucs::scoped_setenv env3("UCX_TEMP", "30");

        car_opts opts(UCS_DEFAULT_ENV_PREFIX, NULL);
        EXPECT_EQ(COLOR_WHITE, opts->color);
        EXPECT_EQ(0U, opts->price);
        EXPECT_EQ(30, opts->temp_front);
        EXPECT_EQ(30, opts->temp_rear);
        EXPECT_EQ(UCS_OK, opts.set("PASSENGERS", "Unknown,third:3"));

        /* coverity[tainted_string_argument] */
        ucs::scoped_setenv env4("UCX_COLOR", "black");
        opts_clone_ptr = new car_opts(opts);
    }

    EXPECT_EQ(COLOR_WHITE, (*opts_clone_ptr)->color);
    EXPECT_EQ(UCS_CONFIG_ALLOW_LIST_ALLOW_ALL, (*opts_clone_ptr)->allow_list.mode);
    EXPECT_EQ(30, (*opts_clone_ptr)->temp_front);
    EXPECT_EQ(30, (*opts_clone_ptr)->temp_rear);
    EXPECT_STREQ("Unknown", (*opts_clone_ptr)->passengers[0]);
    EXPECT_STREQ("Unknown", (*opts_clone_ptr)->passengers[1]);
    EXPECT_STREQ("3", (*opts_clone_ptr)->passengers[2]);
    delete opts_clone_ptr;
}

UCS_TEST_F(test_config, set_get) {
    car_opts opts(UCS_DEFAULT_ENV_PREFIX, NULL);
    EXPECT_EQ(COLOR_RED, opts->color);
    EXPECT_EQ(std::string(color_names[COLOR_RED]),
              std::string(opts.get("COLOR")));

    opts.set("COLOR", "white");
    EXPECT_EQ(COLOR_WHITE, opts->color);
    EXPECT_EQ(std::string(color_names[COLOR_WHITE]),
              std::string(opts.get("COLOR")));

    opts.set("DRIVER_COLOR_ALIAS", "black");
    EXPECT_EQ(COLOR_BLACK, opts->coach.driver_seat.color);
    EXPECT_EQ(std::string(color_names[COLOR_BLACK]),
              std::string(opts.get("COACH_DRIVER_COLOR_ALIAS")));

    opts.set("VIN", "123456");
    EXPECT_EQ(123456UL, opts->vin);

    /* try to set incorrect value - color should not be updated */
    {
        scoped_log_handler log_handler_vars(config_error_suppress);
        opts.set("COLOR", "magenta");
    }

    EXPECT_EQ(COLOR_WHITE, opts->color);
    EXPECT_EQ(std::string(color_names[COLOR_WHITE]),
            std::string(opts.get("COLOR")));
    EXPECT_EQ(1, m_num_errors);
}

UCS_TEST_F(test_config, set_blob)
{
    static const char ucs_cars_prefix[] = UCS_DEFAULT_ENV_PREFIX"CARS_";
    ucs::scoped_setenv ucx_config_dir("UCX_CONFIG_DIR", TEST_CONFIG_DIR);
    car_opts mazda(ucs_cars_prefix, "MAZDA_");

    const std::string val_auto("auto");
    const std::string model2("2"), model3("3"), model6("6"), modelcx5("cx5");
    ucs_status_t status;

    // Check initial values
    ASSERT_EQ(val_auto, mazda.get("ENGINE_MODEL"));
    ASSERT_EQ(std::string("Corvette"), mazda.get("MODEL"));

    // no prefix
    status = mazda.set("MODEL", model2.c_str());
    ASSERT_UCS_OK(status);
    ASSERT_EQ(model2, mazda.get("MODEL"));
    ASSERT_EQ(val_auto, mazda.get("ENGINE_MODEL")); // TODO: Is this expected?

    // with exact prefix
    status = mazda.set("MAZDA_", "MAZDA_MODEL", model3.c_str());
    ASSERT_UCS_OK(status);
    ASSERT_EQ(model3, mazda.get("MODEL"));
    ASSERT_EQ(model3, mazda.get("ENGINE_MODEL")); // TODO: Is this expected?

    // with prefix ignored by wildcard
    status = mazda.set("MAZDA_", "*MODEL", model6.c_str());
    ASSERT_UCS_OK(status);
    ASSERT_EQ(model6, mazda.get("MODEL"));
    ASSERT_EQ(model6, mazda.get("ENGINE_MODEL"));

    // without prefix and with wildcard
    status = mazda.set("*MODEL", modelcx5.c_str());
    ASSERT_UCS_OK(status);
    ASSERT_EQ(modelcx5, mazda.get("MODEL"));
    ASSERT_EQ(modelcx5, mazda.get("ENGINE_MODEL"));
}

UCS_TEST_F(test_config, set_get_with_table_prefix) {
    /* coverity[tainted_string_argument] */
    ucs::scoped_setenv env1("UCX_COLOR", "black");
    /* coverity[tainted_string_argument] */
    ucs::scoped_setenv env2("UCX_CARS_COLOR", "white");

    car_opts opts(UCS_DEFAULT_ENV_PREFIX, "CARS_");
    EXPECT_EQ(COLOR_WHITE, opts->color);
    EXPECT_EQ(std::string(color_names[COLOR_WHITE]),
              std::string(opts.get("COLOR")));
}

UCS_TEST_F(test_config, set_get_with_env_prefix) {
    /* coverity[tainted_string_argument] */
    ucs::scoped_setenv env1("UCX_COLOR", "black");
    /* coverity[tainted_string_argument] */
    ucs::scoped_setenv env2(TEST_ENV_PREFIX "UCX_COLOR", "white");

    car_opts opts(TEST_ENV_PREFIX UCS_DEFAULT_ENV_PREFIX, NULL);
    EXPECT_EQ(COLOR_WHITE, opts->color);
    EXPECT_EQ(std::string(color_names[COLOR_WHITE]),
              std::string(opts.get("COLOR")));
}

UCS_TEST_F(test_config, performance) {

    /* Add stuff to env to presumably make getenv() slower */
    ucs::ptr_vector<ucs::scoped_setenv> env;
    for (unsigned i = 0; i < 300; ++i) {
        env.push_back(new ucs::scoped_setenv(
                        (std::string("MTEST") + ucs::to_string(i)).c_str(),
                        ""));
    }

    /* Now test the time */
    UCS_TEST_TIME_LIMIT(0.05) {
        car_opts opts(UCS_DEFAULT_ENV_PREFIX, NULL);
    }
}

UCS_TEST_F(test_config, unused) {
    ucs::ucx_env_cleanup env_cleanup;

    /* set to warn about unused env vars */
    ucs_global_opts.warn_unused_env_vars = 1;

    const std::string warn_str    = "unused environment variable";
    const std::string unused_var1 = "UCX_UNUSED_VAR1";
    /* coverity[tainted_string_argument] */
    ucs::scoped_setenv env1(unused_var1.c_str(), "unused");

    {
        config_err_exp_str.push_back(warn_str + ": " + unused_var1);
        scoped_log_handler log_handler(config_error_handler);
        car_opts opts(UCS_DEFAULT_ENV_PREFIX, NULL);

        ucs_config_parser_print_env_vars_once(UCS_DEFAULT_ENV_PREFIX);

        config_err_exp_str.pop_back();
    }

    {
        const std::string unused_var2 = TEST_ENV_PREFIX "UNUSED_VAR2";
        /* coverity[tainted_string_argument] */
        ucs::scoped_setenv env2(unused_var2.c_str(), "unused");

        config_err_exp_str.push_back(warn_str + ": " + unused_var2);
        scoped_log_handler log_handler(config_error_handler);
        car_opts opts(TEST_ENV_PREFIX, NULL);

        ucs_config_parser_print_env_vars_once(TEST_ENV_PREFIX);

        config_err_exp_str.pop_back();
    }

    /* reset to not warn about unused env vars */
    ucs_global_opts.warn_unused_env_vars = 0;
}

UCS_TEST_F(test_config, dump) {
    /* aliases must not be counted here */
    test_config_print_opts(UCS_CONFIG_PRINT_CONFIG, 35u);
}

UCS_TEST_F(test_config, dump_hidden) {
    /* aliases must be counted here */
    test_config_print_opts(UCS_CONFIG_PRINT_CONFIG | UCS_CONFIG_PRINT_HIDDEN, 42u);
}

UCS_TEST_F(test_config, dump_hidden_check_alias_name) {
    /* aliases must be counted here */
    test_config_print_opts(UCS_CONFIG_PRINT_CONFIG | UCS_CONFIG_PRINT_HIDDEN |
                                   UCS_CONFIG_PRINT_DOC,
                           42u);

    test_config_print_opts(UCS_CONFIG_PRINT_CONFIG | UCS_CONFIG_PRINT_HIDDEN |
                                   UCS_CONFIG_PRINT_DOC,
                           42u, TEST_ENV_PREFIX);
}

UCS_TEST_F(test_config, deprecated) {
    /* set to warn about unused env vars */
    ucs_global_opts.warn_unused_env_vars = 1;

    const std::string warn_str        = " is deprecated";
    const std::string deprecated_var1 = "UCX_DRIVER";
    /* coverity[tainted_string_argument] */
    ucs::scoped_setenv env1(deprecated_var1.c_str(), "Taxi driver");
    config_err_exp_str.push_back(deprecated_var1 + warn_str);

    {
        scoped_log_handler log_handler(config_error_handler);
        car_opts opts(UCS_DEFAULT_ENV_PREFIX, NULL);
    }

    {
        const std::string deprecated_var2 = "UCX_ENGINE_FUEL_LEVEL";
        /* coverity[tainted_string_argument] */
        ucs::scoped_setenv env2(deprecated_var2.c_str(), "58");
        config_err_exp_str.push_back(deprecated_var2 + warn_str);

        scoped_log_handler log_handler_vars(config_error_handler);
        car_opts opts(UCS_DEFAULT_ENV_PREFIX, NULL);
        config_err_exp_str.pop_back();
    }

    config_err_exp_str.pop_back();

    /* reset to not warn about unused env vars */
    ucs_global_opts.warn_unused_env_vars = 0;
}

UCS_TEST_F(test_config, test_allow_list) {
    const std::string allow_list = "UCX_ALLOW_LIST";

    {
        /* coverity[tainted_string_argument] */
        ucs::scoped_setenv env(allow_list.c_str(), "first,second");

        car_opts opts(UCS_DEFAULT_ENV_PREFIX, NULL);
        EXPECT_EQ(UCS_CONFIG_ALLOW_LIST_ALLOW, opts->allow_list.mode);
        EXPECT_EQ(2, opts->allow_list.array.count);
        EXPECT_EQ(std::string("first"), opts->allow_list.array.names[0]);
        EXPECT_EQ(std::string("second"), opts->allow_list.array.names[1]);
    }

    {
        /* coverity[tainted_string_argument] */
        ucs::scoped_setenv env(allow_list.c_str(), "^first,second");

        car_opts opts(UCS_DEFAULT_ENV_PREFIX, NULL);
        EXPECT_EQ(UCS_CONFIG_ALLOW_LIST_NEGATE, opts->allow_list.mode);
        EXPECT_EQ(2, opts->allow_list.array.count);
        EXPECT_EQ(std::string("first"), opts->allow_list.array.names[0]);
        EXPECT_EQ(std::string("second"), opts->allow_list.array.names[1]);
    }
}

UCS_TEST_F(test_config, test_allow_list_negative)
{
    ucs_config_allow_list_t field;

    EXPECT_EQ(ucs_config_sscanf_allow_list("all,all", &field,
                                           &ucs_config_array_string), 0);
}

UCS_TEST_F(test_config, test_key_value_generic_value) {
    /* coverity[tainted_string_argument] */
    ucs::scoped_setenv env1("UCX_TEMP", "42");

    car_opts opts(UCS_DEFAULT_ENV_PREFIX, NULL);
    EXPECT_EQ(42, opts->temp_front);
    EXPECT_EQ(42, opts->temp_rear);

    EXPECT_EQ(UCS_OK, opts.set("PASSENGERS", "Unknown"));
    EXPECT_STREQ("Unknown", opts->passengers[0]);
    EXPECT_STREQ("Unknown", opts->passengers[1]);
    EXPECT_STREQ("Unknown", opts->passengers[2]);
}

UCS_TEST_F(test_config, test_key_value_mixed) {
    /* coverity[tainted_string_argument] */
    ucs::scoped_setenv env1("UCX_TEMP", "15,rear:51");

    car_opts opts(UCS_DEFAULT_ENV_PREFIX, NULL);
    EXPECT_EQ(15, opts->temp_front);
    EXPECT_EQ(51, opts->temp_rear);

    EXPECT_EQ(UCS_OK, opts.set("PASSENGERS", "Unknown,second:Some Name"));
    EXPECT_STREQ("Unknown", opts->passengers[0]);
    EXPECT_STREQ("Some Name", opts->passengers[1]);
    EXPECT_STREQ("Unknown", opts->passengers[2]);
}

UCS_TEST_F(test_config, test_key_value_empty_string_key) {
    car_opts opts(UCS_DEFAULT_ENV_PREFIX, NULL);

    EXPECT_EQ(UCS_OK, opts.set("PASSENGERS", ""));
    EXPECT_STREQ("", opts->passengers[0]);
    EXPECT_STREQ("", opts->passengers[1]);
    EXPECT_STREQ("", opts->passengers[2]);

    EXPECT_EQ(UCS_OK, opts.set("PASSENGERS", ",second:2"));
    EXPECT_STREQ("", opts->passengers[0]);
    EXPECT_STREQ("2", opts->passengers[1]);
    EXPECT_STREQ("", opts->passengers[2]);
}

UCS_TEST_F(test_config, test_key_value_explicit) {
    /* coverity[tainted_string_argument] */
    ucs::scoped_setenv env1("UCX_TEMP", "rear:51,front:15");

    car_opts opts(UCS_DEFAULT_ENV_PREFIX, NULL);
    EXPECT_EQ(15, opts->temp_front);
    EXPECT_EQ(51, opts->temp_rear);

    EXPECT_EQ(UCS_OK, opts.set("PASSENGERS", "third:3,second:2,first:1"));
    EXPECT_STREQ("1", opts->passengers[0]);
    EXPECT_STREQ("2", opts->passengers[1]);
    EXPECT_STREQ("3", opts->passengers[2]);
}

UCS_TEST_F(test_config, test_key_value_wrong_syntax) {
    car_opts opts(UCS_DEFAULT_ENV_PREFIX, NULL);
    EXPECT_EQ(UCS_OK, opts.set("TEMP", "17"));
    EXPECT_EQ(17, opts->temp_front);
    EXPECT_EQ(17, opts->temp_rear);

    {
        config_err_exp_str = {"key 'unknown' is not supported",
                              "Invalid value for TEMP: 'unknown:10'"};

        scoped_log_handler log_handler_vars(config_error_handler);
        EXPECT_EQ(UCS_ERR_INVALID_PARAM, opts.set("TEMP", "unknown:10"));
        EXPECT_EQ(17, opts->temp_front);
        EXPECT_EQ(17, opts->temp_rear);

        config_err_exp_str.clear();
    }
    {
        config_err_exp_str = {"Invalid value for TEMP: ''"};

        scoped_log_handler log_handler_vars(config_error_handler);
        EXPECT_EQ(UCS_ERR_INVALID_PARAM, opts.set("TEMP", ""));
        EXPECT_EQ(17, opts->temp_front);
        EXPECT_EQ(17, opts->temp_rear);

        config_err_exp_str.clear();
    }
    {
        config_err_exp_str = {"no value configured for key 'first'",
                              "Invalid value for PASSENGERS: "};

        scoped_log_handler log_handler_vars(config_error_handler);
        EXPECT_EQ(UCS_ERR_INVALID_PARAM, opts.set("PASSENGERS", "second:2"));
        EXPECT_STREQ("None", opts->passengers[0]);
        EXPECT_STREQ("None", opts->passengers[1]);
        EXPECT_STREQ("None", opts->passengers[2]);

        config_err_exp_str.clear();
    }
}

UCS_TEST_F(test_config, test_key_value_dump_config) {
    /* coverity[tainted_string_argument] */
    ucs::scoped_setenv env1("UCX_TEMP", "rear:16,front:17");

    car_opts opts(UCS_DEFAULT_ENV_PREFIX, NULL);
    std::string dump = opts.dump(UCS_CONFIG_PRINT_CONFIG);

    size_t it = dump.find("\nUCX_TEMP=front:17,rear:16\n");
    EXPECT_NE(it, std::string::npos);

    it = dump.find("\nUCX_PASSENGERS=first:None,second:None,third:None\n");
    EXPECT_NE(it, std::string::npos);
}

UCS_TEST_F(test_config, test_key_value_dump_full) {
    /* coverity[tainted_string_argument] */
    ucs::scoped_setenv env1("UCX_TEMP", "rear:16,front:17");

    car_opts opts(UCS_DEFAULT_ENV_PREFIX, NULL);

    int flags = UCS_CONFIG_PRINT_CONFIG |
                UCS_CONFIG_PRINT_HEADER |
                UCS_CONFIG_PRINT_DOC    |
                UCS_CONFIG_PRINT_HIDDEN |
                UCS_CONFIG_PRINT_COMMENT_DEFAULT;
    std::string dump = opts.dump((ucs_config_print_flags_t)flags);

    size_t it = dump.find("# Temperature\n"
                          "#  front     - front temperature\n"
                          "#  rear      - rear temperature\n"
                          "#\n"
                          "# syntax:    comma-separated list of value or key:"
                          "value pairs, where key is one of [front,rear] and "
                          "value is: unsigned integer. A value without a key is"
                          " the default.\n"
                          "#\n"
                          "UCX_TEMP=front:17,rear:16\n");
    EXPECT_NE(it, std::string::npos);
}

UCS_TEST_F(test_config, test_config_dump_filtered) {
    car_opts opts(UCS_DEFAULT_ENV_PREFIX, nullptr);

    const std::string dump1 = opts.dump(UCS_CONFIG_PRINT_CONFIG);
    const char filter[]     = "TIME_";
    const std::string dump2 = opts.dump(UCS_CONFIG_PRINT_CONFIG, filter);

    EXPECT_NE(dump1.size(), dump2.size());
    EXPECT_NE(dump1.find("TIME_"), std::string::npos);
    EXPECT_NE(dump2.find("TIME_"), std::string::npos);
    EXPECT_NE(dump1.find("TRANSMISSION"), std::string::npos);
    EXPECT_EQ(dump2.find("TRANSMISSION"), std::string::npos);
}

UCS_TEST_F(test_config, test_config_file) {
    /* coverity[tainted_string_argument] */
    ucs::scoped_setenv env1("UCX_BRAND", "Ford");

    car_opts opts(UCS_DEFAULT_ENV_PREFIX, NULL);

    /* Option parsing from INI file */
    EXPECT_EQ(100, opts->price);

    /* Overriding INI file by env vars */
    EXPECT_EQ(std::string("Ford"), std::string(opts->brand));
}

UCS_TEST_F(test_config, test_config_file_parse_files) {
    /* coverity[tainted_string_argument] */
    ucs::scoped_setenv ucx_config_dir("UCX_CONFIG_DIR", TEST_CONFIG_DIR);

    car_opts_t opts;
    ucs_status_t status;

    ucs_config_parse_config_file(TEST_CONFIG_DIR, TEST_CONFIG_FILE, 1);

    ucs_config_global_list_entry_t entry;
    entry.table  = car_opts_table;
    entry.name   = "cars";
    entry.prefix = NULL;
    entry.size   = sizeof(car_opts_t);
    entry.flags  = 0;

    status = ucs_config_parser_fill_opts(&opts, &entry, UCS_DEFAULT_ENV_PREFIX,
                                         0);
    EXPECT_EQ(200, opts.price);
    EXPECT_EQ(15, opts.temp_front);
    EXPECT_EQ(10, opts.temp_rear);
    ucs_config_parser_release_opts(&opts, car_opts_table);

    ucs_config_parse_config_files();
    status = ucs_config_parser_fill_opts(&opts, &entry, UCS_DEFAULT_ENV_PREFIX,
                                         0);
    ASSERT_UCS_OK(status);

    /* Verify ucs_config_parse_config_files() overrides config */
    EXPECT_EQ(100, opts.price);
    ucs_config_parser_release_opts(&opts, car_opts_table);
}
