#include "vx300_impl.hpp"

#include <uhd/exception.hpp>
#include <uhd/types/dict.hpp>
#include <uhd/types/ranges.hpp>
#include <uhd/types/sensors.hpp>
#include <uhd/types/time_spec.hpp>
#include <uhd/usrp/dboard_eeprom.hpp>
#include <uhd/usrp/mboard_eeprom.hpp>
#include <uhd/usrp/subdev_spec.hpp>
#include <uhd/utils/log.hpp>
#include <uhd/utils/static.hpp>

#include <complex>
#include <memory>
#include <string>
#include <vector>

using namespace uhd;
using namespace uhd::usrp;
using namespace uhd::usrp::vx300;

static constexpr const char* VX300_TYPE    = "vx300";
static constexpr const char* VX300_PRODUCT = "VX300 Virtual X300-like Device";
static constexpr const char* VX300_NAME    = "VX300";

/***********************************************************************
 * Discovery
 **********************************************************************/

static device_addrs_t vx300_find(const device_addr_t& hint)
{
    device_addrs_t addrs;

    /*
     * 如果用户指定了 type，但不是 vx300，则不返回。
     * 如果用户没有指定 type，则允许返回虚拟设备。
     */
    if (hint.has_key("type") && hint.get("type") != VX300_TYPE) {
        return addrs;
    }

    /*
     * 可选：如果用户指定了 serial/name，也做匹配过滤。
     */
    const std::string serial = hint.get("serial", "VX3000001");
    const std::string name   = hint.get("name", VX300_NAME);

    if (hint.has_key("serial") && hint.get("serial") != serial) {
        return addrs;
    }

    device_addr_t dev_addr;
    dev_addr["type"]     = VX300_TYPE;
    dev_addr["product"]  = VX300_PRODUCT;
    dev_addr["name"]     = name;
    dev_addr["serial"]   = serial;
    dev_addr["resource"] = "virtual";
    dev_addr["addr"]     = hint.get("addr", "virtual");

    addrs.push_back(dev_addr);
    return addrs;
}

static device::sptr vx300_make(const device_addr_t& device_addr)
{
    return device::sptr(new vx300_impl(device_addr));
}

UHD_STATIC_BLOCK(register_vx300_device)
{
    /*
     * 新版 UHD register_device() 需要第三个参数 device::USRP。
     * 如果你的 UHD 是较老版本，编译报参数数量错误时，把第三个参数删掉即可。
     */
    device::register_device(&vx300_find, &vx300_make, device::USRP);
}

/***********************************************************************
 * Constructor
 **********************************************************************/

vx300_impl::vx300_impl(const device_addr_t& args) : _args(args)
{
    _type = device::USRP;
    _tree = property_tree::make();

    setup_tree();
}

void vx300_impl::setup_tree()
{
    _tree->create<std::string>("/name").set(VX300_PRODUCT);

    setup_mboard_tree();
    setup_rx_tree();
    setup_tx_tree();
}

/***********************************************************************
 * Motherboard property tree
 **********************************************************************/

void vx300_impl::setup_mboard_tree()
{
    const std::string mb = "/mboards/0";

    mboard_eeprom_t mb_eeprom;
    mb_eeprom["name"]   = VX300_NAME;
    mb_eeprom["serial"] = _args.get("serial", "VX3000001");
    mb_eeprom["product"] = VX300_PRODUCT;

    _tree->create<mboard_eeprom_t>(mb + "/eeprom").set(mb_eeprom);

    _tree->create<std::string>(mb + "/name").set(VX300_NAME);
    _tree->create<std::string>(mb + "/serial").set(_args.get("serial", "VX3000001"));
    _tree->create<std::string>(mb + "/product").set(VX300_PRODUCT);

    _tree->create<double>(mb + "/tick_rate").set(100e6);
    _tree->create<meta_range_t>(mb + "/tick_rate/range").set(meta_range_t(100e6, 100e6));

    _tree->create<std::vector<std::string>>(mb + "/clock_source/options")
        .set(std::vector<std::string>{"internal"});
    _tree->create<std::string>(mb + "/clock_source/value").set("internal");

    _tree->create<std::vector<std::string>>(mb + "/time_source/options")
        .set(std::vector<std::string>{"internal"});
    _tree->create<std::string>(mb + "/time_source/value").set("internal");

    _tree->create<time_spec_t>(mb + "/time/now").set(time_spec_t(0.0));
    _tree->create<time_spec_t>(mb + "/time/pps").set(time_spec_t(0.0));

    _tree->create<std::vector<std::string>>(mb + "/sensors")
        .set(std::vector<std::string>{"ref_locked"});
    _tree->create<sensor_value_t>(mb + "/sensors/ref_locked")
        .set(sensor_value_t("ref_locked", true, "locked", "unlocked"));

    /*
     * 一个虚拟 dboard，名字为 A。
     * multi_usrp 默认会从 /mboards/0/dboards 下找默认 subdev。
     */
    _tree->create<std::string>(mb + "/dboards/A/name").set("VX300 Dummy Dboard");

    subdev_spec_t rx_spec("A:0");
    subdev_spec_t tx_spec("A:0");

    _tree->create<subdev_spec_t>(mb + "/rx_subdev_spec").set(rx_spec);
    _tree->create<subdev_spec_t>(mb + "/tx_subdev_spec").set(tx_spec);
}

/***********************************************************************
 * RX property tree
 **********************************************************************/

void vx300_impl::setup_rx_tree()
{
    const std::string rx_fe  = "/mboards/0/dboards/A/rx_frontends/0";
    const std::string rx_dsp = "/mboards/0/rx_dsps/0";

    _tree->create<std::string>(rx_fe + "/name").set("VX300 RX0");

    _tree->create<meta_range_t>(rx_fe + "/freq/range").set(meta_range_t(70e6, 6e9));
    _tree->create<double>(rx_fe + "/freq/value").set(100e6);

    _tree->create<std::vector<std::string>>(rx_fe + "/antenna/options")
        .set(std::vector<std::string>{"RX"});
    _tree->create<std::string>(rx_fe + "/antenna/value").set("RX");

    _tree->create<std::vector<std::string>>(rx_fe + "/gains")
        .set(std::vector<std::string>{"PGA"});
    _tree->create<meta_range_t>(rx_fe + "/gains/PGA/range").set(meta_range_t(0.0, 30.0, 1.0));
    _tree->create<double>(rx_fe + "/gains/PGA/value").set(0.0);

    _tree->create<meta_range_t>(rx_fe + "/bandwidth/range")
        .set(meta_range_t(1e6, 100e6));
    _tree->create<double>(rx_fe + "/bandwidth/value").set(20e6);

    _tree->create<std::vector<std::string>>(rx_fe + "/sensors")
        .set(std::vector<std::string>{});

    /*
     * DSP 部分。
     * multi_usrp 会用 RF frontend freq 和 DSP freq 合成最终 center freq。
     */
    _tree->create<meta_range_t>(rx_dsp + "/rate/range").set(meta_range_t(1e6, 100e6));
    _tree->create<double>(rx_dsp + "/rate/value").set(1e6);

    _tree->create<meta_range_t>(rx_dsp + "/freq/range").set(meta_range_t(-50e6, 50e6));
    _tree->create<double>(rx_dsp + "/freq/value").set(0.0);

    _tree->create<meta_range_t>(rx_dsp + "/freq/output").set(meta_range_t(-50e6, 50e6));
}

/***********************************************************************
 * TX property tree
 **********************************************************************/

void vx300_impl::setup_tx_tree()
{
    const std::string tx_fe  = "/mboards/0/dboards/A/tx_frontends/0";
    const std::string tx_dsp = "/mboards/0/tx_dsps/0";

    _tree->create<std::string>(tx_fe + "/name").set("VX300 TX0");

    _tree->create<meta_range_t>(tx_fe + "/freq/range").set(meta_range_t(70e6, 6e9));
    _tree->create<double>(tx_fe + "/freq/value").set(100e6);

    _tree->create<std::vector<std::string>>(tx_fe + "/antenna/options")
        .set(std::vector<std::string>{"TX"});
    _tree->create<std::string>(tx_fe + "/antenna/value").set("TX");

    _tree->create<std::vector<std::string>>(tx_fe + "/gains")
        .set(std::vector<std::string>{"PGA"});
    _tree->create<meta_range_t>(tx_fe + "/gains/PGA/range").set(meta_range_t(0.0, 30.0, 1.0));
    _tree->create<double>(tx_fe + "/gains/PGA/value").set(0.0);

    _tree->create<meta_range_t>(tx_fe + "/bandwidth/range")
        .set(meta_range_t(1e6, 100e6));
    _tree->create<double>(tx_fe + "/bandwidth/value").set(20e6);

    _tree->create<std::vector<std::string>>(tx_fe + "/sensors")
        .set(std::vector<std::string>{});

    _tree->create<meta_range_t>(tx_dsp + "/rate/range").set(meta_range_t(1e6, 100e6));
    _tree->create<double>(tx_dsp + "/rate/value").set(1e6);

    _tree->create<meta_range_t>(tx_dsp + "/freq/range").set(meta_range_t(-50e6, 50e6));
    _tree->create<double>(tx_dsp + "/freq/value").set(0.0);

    _tree->create<meta_range_t>(tx_dsp + "/freq/output").set(meta_range_t(-50e6, 50e6));
}

/***********************************************************************
 * Stream API
 **********************************************************************/

rx_streamer::sptr vx300_impl::get_rx_stream(const stream_args_t&)
{
    throw uhd::not_implemented_error(
        "VX300 virtual device does not implement RX streaming yet");
}

tx_streamer::sptr vx300_impl::get_tx_stream(const stream_args_t&)
{
    throw uhd::not_implemented_error(
        "VX300 virtual device does not implement TX streaming yet");
}

bool vx300_impl::recv_async_msg(async_metadata_t&, double)
{
    return false;
}