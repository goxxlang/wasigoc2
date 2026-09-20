#include "test.h"

#include "wow/driver.h"
#include "wow/occupancy.h"

#include <string>

TEST(DriverEntryLoadsSysImages) {
  wow::Driver d;
  EXPECT(!d.replaces_host_ntos());
  EXPECT_EQ(d.DriverEntry("wowwin32"), WOW_RESULT_OK);
  EXPECT(d.loaded());
  EXPECT(d.table().size() == 3);
  EXPECT(d.table().OfKind(wow::DriverKind::kOccupancy) != nullptr);
  EXPECT(d.table().OfKind(wow::DriverKind::kNtoskrnl)->image == "ntoskrnl.exe");
  auto json = d.GetDriverInfoJson();
  EXPECT(json.find("wowwin32.sys") != std::string::npos);
  EXPECT(json.find("\"isolation\":\"sys\"") != std::string::npos);
  EXPECT(json.find("\"replaces_host_ntos\":false") != std::string::npos);
}

TEST(DriverAddStartControl) {
  wow::Driver d;
  EXPECT_EQ(d.AddDevice(), WOW_RESULT_OK);
  EXPECT_EQ(d.StartDevice(), WOW_RESULT_OK);
  EXPECT(d.started());
  EXPECT_EQ(d.DeviceControl(WOW_IOCTL_MAP, {}), WOW_RESULT_OK);
  EXPECT(d.table().OfKind(wow::DriverKind::kOccupancy)->mapped);
  EXPECT(d.table().OfKind(wow::DriverKind::kOccupancy)->ioctls == 1u);
}

TEST(UnloadLeavesHostNtos) {
  wow::Driver d;
  d.DriverEntry("wowwin32");
  int ntos = d.table().OfKind(wow::DriverKind::kNtoskrnl)->id;
  int occ = d.table().OfKind(wow::DriverKind::kOccupancy)->id;
  EXPECT_EQ(d.Unload(ntos), WOW_RESULT_FAILED_PRECONDITION);
  EXPECT_EQ(d.Unload(occ), WOW_RESULT_OK);
  EXPECT(d.table().OfKind(wow::DriverKind::kOccupancy) == nullptr);
  EXPECT(d.table().OfKind(wow::DriverKind::kNtoskrnl) != nullptr);
}

TEST(DriverObjectIsWindowsSys) {
  wow::Driver d;
  EXPECT_EQ(d.DriverEntry("wowwin32"), WOW_RESULT_OK);
  EXPECT(d.object().major_function[WOW_IRP_MJ_CREATE]);
  EXPECT(d.object().major_function[WOW_IRP_MJ_CLOSE]);
  EXPECT(d.object().major_function[WOW_IRP_MJ_DEVICE_CONTROL]);
  EXPECT(d.object().major_function[WOW_IRP_MJ_PNP]);
  EXPECT(d.object().service_type == WOW_SERVICE_KERNEL_DRIVER);
  EXPECT_EQ(d.AddDevice(), WOW_RESULT_OK);
  EXPECT(d.device().nt_name == "\\Device\\WowWin32");
  EXPECT(d.device().device_type == WOW_FILE_DEVICE_UNKNOWN);
  EXPECT(d.device().flags == WOW_DO_BUFFERED_IO);
  EXPECT_EQ(d.StartDevice(), WOW_RESULT_OK);
  EXPECT(d.device().started);
  EXPECT(d.last_irp().major == WOW_IRP_MJ_PNP);
  EXPECT(d.last_irp().minor == WOW_IRP_MN_START_DEVICE);
  auto json = d.GetDriverInfoJson();
  EXPECT(json.find("driver_object") != std::string::npos);
  EXPECT(json.find("device_object") != std::string::npos);
  EXPECT(json.find("irp_mj_device_control") != std::string::npos);
  EXPECT(json.find("WowWin32") != std::string::npos);
}

TEST(OccupyDriverHop) {
  wow::Occupancy o;
  EXPECT_EQ(o.OccupyDriver("wowwin32"), WOW_RESULT_OK);
  EXPECT(o.win32_driver().loaded());
  EXPECT(o.win32_driver().started());
  EXPECT(o.table().OfKind(wow::SurfaceKind::kOccupancyDriver).size() == 1);
  auto json = o.StatusJson();
  EXPECT(json.find("wowwin32.sys") != std::string::npos);
  EXPECT(json.find("\"isolation\":\"sys\"") != std::string::npos);
  EXPECT(o.Called("NtLoadDriver"));
  EXPECT(o.Called("GetProcAddress"));
  EXPECT(o.Called("ZwLoadDriver"));
  std::string reply;
  std::string err;
  EXPECT(o.DispatchTopic("getDriverInfo", "", &reply, &err));
  EXPECT(reply.find("ntoskrnl.exe") != std::string::npos);
  EXPECT(o.DispatchTopic("deviceControl", "{\"data\":\"574f57\"}", &reply,
                         &err));
}

TEST(OccupySysShellTtyNet) {
  wow::Occupancy o;
  EXPECT_EQ(o.OccupySys("sys"), WOW_RESULT_OK);
  EXPECT(o.sys());
  EXPECT_EQ(o.OccupyHostNtos("ntoskrnl"), WOW_RESULT_OK);
  EXPECT_EQ(o.OccupyCmd("cmd.exe"), WOW_RESULT_OK);
  EXPECT(o.cmd());
  EXPECT_EQ(o.OccupyLinux("linux"), WOW_RESULT_OK);
  EXPECT(o.linux());
  EXPECT_EQ(o.OccupyWasmtty("wasmtty"), WOW_RESULT_OK);
  EXPECT(o.wasmtty());
  EXPECT_EQ(o.OccupyGocvm("wasigocvm"), WOW_RESULT_OK);
  EXPECT(o.gocvm());
  EXPECT(o.StatusJson().find("\"toolkit\":\"gocvm\"") != std::string::npos);
  EXPECT_EQ(o.OccupyNet("wns"), WOW_RESULT_OK);
  EXPECT(o.net());
  auto json = o.StatusJson();
  EXPECT(json.find("\"permission\":\"sys\"") != std::string::npos);
  EXPECT(json.find("sctp-rpc") != std::string::npos);
  std::string reply;
  std::string err;
  EXPECT(o.DispatchTopic("win32.ttyHello", "", &reply, &err));
  EXPECT(reply.find("\"magic\":\"WTTY\"") != std::string::npos);
  EXPECT(o.DispatchTopic("win32.ttyPing", "", &reply, &err));
  EXPECT(reply.find("pong") != std::string::npos);
  EXPECT(!o.DispatchTopic("os.exec", "cmd.exe", &reply, &err));
  EXPECT(!o.DispatchTopic("tty.hello", "", &reply, &err));
  EXPECT(!o.DispatchTopic("net.call", "", &reply, &err));
  EXPECT(!o.DispatchTopic("vtpm.attest", "", &reply, &err));
}
