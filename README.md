**inverse esb opus dual channel demo**

This sample is based on NCS3.0.2, it integrates OPUS encode/decode, it can support two ptx(microphones), audio stream will be transmitted through inverse esb protocol to dongle.

The audio parameter is: 16KHZ, 16bit, dual channel, 10ms per frame. Inverse esb poll frequency is 5ms, which means PTX will send packet every 5ms if it received the poll packet.

As nRF54l15 doesn't support usb ,so in this sample, it will be the microphone to sample audio stream then transmit it to dongle(nRF52840DK or nRF54L15DK). If you choose nRF52840DK as dongle, it will send the received audio stream to usb audio class driver, then you can record the audio with "Audacity" app.

Of course you can choose nRF54L15 as dongle then play it with IIS speaker, but this sample didn't implement IIS output feature, so you can only print log to check if packet lost.

Note:

There are two inverse esb library in this project, one is for 54L series, which located in "/lib/inv_esb_lib" folder; the other is for nRF52 series, it located in "dongle_52s/lib/inv_esb_lib". it is only used to test usb audio when you choose 52840dk as dongle. 

Requirements
************

- nRF Connect SDK v3.0.0
- nRF52/nRF54L series development kit