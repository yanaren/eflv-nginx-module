Name
====

eflv-nginx-module - the module provides pseudo-streaming server-side support for Flash Video (FLV) files.

*This module is not distributed with the Nginx source.* See [the installation instructions](#installation).


Table of Contents
=================

* [Description](#description)
* [Installation](#installation)
* [Example](#example)
* [Directives](#directives)
    * [tflv](#tflv)
    * [sflv](#sflv)
* [Changes](#changes)
* [Copyright and License](#copyright-and-license)
* [See Also](#see-also)

Description
===========

The *hls-nginx-module* module provides HTTP Live Streaming (HLS) server-side support for H.264/AAC files. Such files typically have the .mp4, .m4v, or .m4a filename extensions.

nginx supports two URIs for each MP4 file:

 * The playlist URI that ends with “.m3u8” and accepts the optional “len” argument that defines the fragment length in seconds;
 * The fragment URI that ends with “.ts” and accepts “start” and “end” arguments that define fragment boundaries in seconds.

[Back to TOC](#table-of-contents)


Installation
============

```bash

 wget 'http://nginx.org/download/nginx-1.9.9.tar.gz'
 tar -xzvf nginx-1.9.9.tar.gz
 cd nginx-1.9.9/

 # Here we assume Nginx is to be installed under /opt/nginx/.
 ./configure --prefix=/opt/nginx \
         --add-module=/path/to/eflv-nginx-module

 make -j2
 make install
```

Example Configuration
=====================
```Example
location /video1/ {
    tflv;
}

location /video2/ {
    sflv;
}
```

With this configuration, the following URIs are supported for the “/var/video/test.mp4” file:

```url
http://video.example.com/video1/test.flv?start=1.000&end=2.200
http://video.example.com/video2/test.flv?start=1.000&end=2.200
```

[Back to TOC](#table-of-contents)

Directives
===========
tflv
--------------------
**syntax:** *tflv*

**default:** *-*

**context:** *http, server, location, location if*

Turns on module processing in a surrounding location with time.


sflv
--------------------
**syntax:** *sflv*

**default:** *-*

**context:** *http, server, location, location if*

Turns on module processing in a surrounding location with position.


Copyright and License
=====================

This module is licensed under the BSD license.

Copyright (C) 2011-2016, Xunen <leixunen@gmail.com> and others.
Copyright (C) 2011-2016, Leevid Inc.

All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

[Back to TOC](#table-of-contents)


See Also
========

[Back to TOC](#table-of-contents)


