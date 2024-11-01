# Simple example

This is a simple example that builds as follows:
1) Connect the include file Hypervisor.hpp


2) Setting up callbacks for the main changeable structures of the hypervisor


3) Declare (in my case, I wanted to declare a global object) the object, then check the VMX support, call the VirtualizeAllProcessors method. In my case we have additional layer over VirtualizeAllProcessors method this layer is HyperVisor::Virtualize()


4) The hypervisor has virtualized the existing system


We can inject it, for example, through OSR Loader or through my header library https://github.com/Nitr0-G/DriverLoader or through self loader https://github.com/Nitr0-G/VMX-Hypervisor/blob/main/example/Simple%20example/AddinApplication/Src/SelfLoader.cpp#L3
