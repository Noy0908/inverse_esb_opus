**inverse esb adpcm demo**

This sample is based on NCS3.0.2, it integrates adpcm encode/decode, it can support two ptx(microphones), audio stream will be transmitted through inverse esb protocol to dongle.

The audio parameter is: 16KHZ, 16bit, single channel, 5ms per frame.
Inverse esb poll frequency is 2ms, which means PTX will send packet every 2ms if it received the poll packet.

In this sample,nRF54l15 will be the microphone to sample audio stream then transmit it to dongle(nRF52840DK).  
you can choose thingy52 as microphone, it has integrate MIC in  thingy52DK, just build the "esb_ptx_thingy52" project.

nRF52840DK will be the dongle, it will send the received audio stream to usb audio class driver, then you can record the audio with "Audacity" app.

Of course you can choose nRF54L15 as dongle then play it with IIS speaker, but this sample didn't implement IIS output feature, so you can only print log to check if packet lost.


Note:

There are two inverse esb library in this project, one is for 54L series, which located in "esb_ptx_54s/lib/inv_esb_lib" folder;
the other is for nRF52 series, it located in "lib/inv_esb_lib". it is used to nRF52 series, namely the "dongle" and "esb_ptx_thingy52" projects.

Requirements
************

- nRF Connect SDK v3.0.2
- nRF52/nRF54L series development kit or thingy52 DK