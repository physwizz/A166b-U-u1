#include "../../comm/shub_comm.h"
#include "../../sensor/pressure.h"
#include "../../sensormanager/shub_sensor.h"
#include "../../sensormanager/shub_sensor_manager.h"
#include "../../utility/shub_utility.h"
#include "pressure_factory.h"

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

#define LPS22DF_NAME   "LPS22DFTR"

static ssize_t pressure_lps22df_temperature_show(char *buf)
{
	s32 temperature = 0;
	s32 float_temperature = 0;
	s32 temp = 0;
	struct pressure_event *sensor_value = (struct pressure_event *)(get_sensor_event(SENSOR_TYPE_PRESSURE)->value);

	temp = (s32)(sensor_value->temperature);
	temperature = (4250) + ((temp / (120 * 4)) * 100); //(42.5f) + (temperature/(120 * 4));
	float_temperature = ((temperature % 100) > 0 ? (temperature % 100) : -(temperature % 100));

	return sprintf(buf, "%d.%02d\n", (temperature / 100), float_temperature);
}

static ssize_t pressure_lps22df_esn_show(char *buf)
{
	char *buffer = NULL;
	int buffer_length = 0;
	int ret = 0;

	ret = shub_send_command_wait(CMD_GETVALUE, SENSOR_TYPE_PRESSURE, PRESSURE_SUBCMD_ESN, 1000, NULL, 0,
					&buffer, &buffer_length, true);
	if (ret < 0) {
		shub_errf("shub_send_command_wait Fail %d", ret);
		return ret;
	}

	ret = sprintf(buf, "%s\n", buffer);

	if (buffer != NULL)
		kfree(buffer);

	return ret;
}

struct pressure_factory_chipset_funcs pressure_lps22df_ops = {
	.temperature_show =pressure_lps22df_temperature_show,
	.esn_show = pressure_lps22df_esn_show,
};

struct pressure_factory_chipset_funcs *get_pressure_lps22df_chipset_func(char *name)
{
	if (strcmp(name, LPS22DF_NAME) != 0)
		return NULL;

	return &pressure_lps22df_ops;
}
