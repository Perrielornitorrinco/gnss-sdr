/*!
 * \file gr_complex_ip_packet_source.cc
 *
 * \brief Receives ip frames containing samples in UDP frame encapsulation
 * using a high performance packet capture library (libpcap)
 * \author Javier Arribas jarribas (at) cttc.es
 *
 * -----------------------------------------------------------------------------
 *
 * GNSS-SDR is a Global Navigation Satellite System software-defined receiver.
 * This file is part of GNSS-SDR.
 *
 * Copyright (C) 2010-2020  (see AUTHORS file for a list of contributors)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * -----------------------------------------------------------------------------
 */


#include "gr_complex_ip_packet_source.h"
#include <gnuradio/io_signature.h>
#include <array>
#include <cstdint>
#include <utility>
#if HAS_GENERIC_LAMBDA
#else
#include <boost/bind/bind.hpp>
#endif

const int FIFO_SIZE = 1472000;


/* 4 bytes IP address */
typedef struct gr_ip_address
{
    u_char byte1;
    u_char byte2;
    u_char byte3;
    u_char byte4;
} gr_ip_address;


/* IPv4 header */
typedef struct gr_ip_header
{
    u_char ver_ihl;          // Version (4 bits) + Internet header length (4 bits)
    u_char tos;              // Type of service
    u_short tlen;            // Total length
    u_short identification;  // Identification
    u_short flags_fo;        // Flags (3 bits) + Fragment offset (13 bits)
    u_char ttl;              // Time to live
    u_char proto;            // Protocol
    u_short crc;             // Header checksum
    gr_ip_address saddr;     // Source address
    gr_ip_address daddr;     // Destination address
    u_int op_pad;            // Option + Padding
} gr_ip_header;


/* UDP header*/
typedef struct gr_udp_header
{
    u_short sport;  // Source port
    u_short dport;  // Destination port
    u_short len;    // Datagram length
    u_short crc;    // Checksum
} gr_udp_header;


Gr_Complex_Ip_Packet_Source::sptr
Gr_Complex_Ip_Packet_Source::make(std::string src_device,
    const std::string &origin_address,
    int udp_port,
    int udp_packet_size,
    int n_baseband_channels,
    const std::string &wire_sample_type,
    size_t item_size,
    bool IQ_swap_)
{
    return gnuradio::get_initial_sptr(new Gr_Complex_Ip_Packet_Source(std::move(src_device),
        origin_address,
        udp_port,
        udp_packet_size,
        n_baseband_channels,
        wire_sample_type,
        item_size,
        IQ_swap_));
}


/*
 * The private constructor
 */
Gr_Complex_Ip_Packet_Source::Gr_Complex_Ip_Packet_Source(std::string src_device,
    __attribute__((unused)) const std::string &origin_address,
    int udp_port,
    int udp_packet_size,
    int n_baseband_channels,
    const std::string &wire_sample_type,
    size_t item_size,
    bool IQ_swap_)
    : gr::sync_block("gr_complex_ip_packet_source",
          gr::io_signature::make(0, 0, 0),
          gr::io_signature::make(1, 4, item_size))  // 1 to 4 baseband complex channels
{
    std::cout << "Start Ethernet packet capture\n";

    d_n_baseband_channels = n_baseband_channels;
    if (wire_sample_type == "cbyte")
        {
            d_wire_sample_type = 1;
            d_bytes_per_sample = d_n_baseband_channels * 2;
        }
    else if (wire_sample_type == "c4bits")
        {
            d_wire_sample_type = 2;
            d_bytes_per_sample = d_n_baseband_channels;
        }
    else if (wire_sample_type == "cfloat")
        {
            d_wire_sample_type = 3;
            d_bytes_per_sample = d_n_baseband_channels * 8;
        }
    else
        {
            std::cout << "Unknown wire sample type\n";
            exit(0);
        }
    std::cout << "d_wire_sample_type:" << d_wire_sample_type << '\n';
    d_src_device = std::move(src_device);
    d_udp_port = udp_port;
    d_udp_payload_size = udp_packet_size;
    d_fifo_full = false;

    // allocate signal samples buffer
    fifo_buff = new char[FIFO_SIZE];
    fifo_read_ptr = 0;
    fifo_write_ptr = 0;
    fifo_items = 0;
    d_item_size = item_size;
    d_IQ_swap = IQ_swap_;
    d_sock_raw = 0;
    d_pcap_thread = nullptr;
    descr = nullptr;

    memset(reinterpret_cast<char *>(&si_me), 0, sizeof(si_me));
}


// Called by gnuradio to enable drivers, etc for i/o devices.
bool Gr_Complex_Ip_Packet_Source::start()
{
    std::cout << "gr_complex_ip_packet_source START\n";
    // open the ethernet device
    if (open() == true)
        {
            // start pcap capture thread
            d_pcap_thread = new boost::thread(
#if HAS_GENERIC_LAMBDA
                [this] { my_pcap_loop_thread(descr); });
#else
                boost::bind(&Gr_Complex_Ip_Packet_Source::my_pcap_loop_thread, this, descr));
#endif
            return true;
        }
    return false;
}


// Called by gnuradio to disable drivers, etc for i/o devices.
bool Gr_Complex_Ip_Packet_Source::stop()
{
    std::cout << "gr_complex_ip_packet_source STOP\n";
    if (descr != nullptr)
        {
            pcap_breakloop(descr);
            d_pcap_thread->join();
            pcap_close(descr);
        }
    return true;
}


bool Gr_Complex_Ip_Packet_Source::open()
{
    std::array<char, PCAP_ERRBUF_SIZE> errbuf{};
    boost::mutex::scoped_lock lock(d_mutex);  // hold mutex for duration of this function
    // open device for reading
    descr = pcap_open_live(d_src_device.c_str(), 1500, 1, 1000, errbuf.data());
    if (descr == nullptr)
        {
            std::cout << "Error opening Ethernet device " << d_src_device << '\n';
            std::cout << "Fatal Error in pcap_open_live(): " << std::string(errbuf.data()) << '\n';
            return false;
        }
    // bind UDP port to avoid automatic reply with ICMP port unreachable packets from kernel
    d_sock_raw = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (d_sock_raw == -1)
        {
            std::cout << "Error opening UDP socket\n";
            return false;
        }

    // zero out the structure
    memset(reinterpret_cast<char *>(&si_me), 0, sizeof(si_me));

    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(d_udp_port);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind socket to port
    if (bind(d_sock_raw, reinterpret_cast<struct sockaddr *>(&si_me), sizeof(si_me)) == -1)
        {
            std::cout << "Error opening UDP socket\n";
            return false;
        }
    return true;
}


Gr_Complex_Ip_Packet_Source::~Gr_Complex_Ip_Packet_Source()
{
    if (d_pcap_thread != nullptr)
        {
            delete d_pcap_thread;
        }
    delete fifo_buff;
    std::cout << "Stop Ethernet packet capture\n";
}


void Gr_Complex_Ip_Packet_Source::static_pcap_callback(u_char *args, const struct pcap_pkthdr *pkthdr,
    const u_char *packet)
{
    auto *bridge = reinterpret_cast<Gr_Complex_Ip_Packet_Source *>(args);
    bridge->pcap_callback(args, pkthdr, packet);
}


void Gr_Complex_Ip_Packet_Source::pcap_callback(__attribute__((unused)) u_char *args, __attribute__((unused)) const struct pcap_pkthdr *pkthdr,
    const u_char *packet)
{
    boost::mutex::scoped_lock lock(d_mutex);  // hold mutex for duration of this function

    const gr_ip_header *ih;
    const gr_udp_header *uh;

    // eth frame parameters
    // **** UDP RAW PACKET DECODER ****
    if ((packet[12] == 0x08) & (packet[13] == 0x00))  // IP FRAME
        {
            // retrieve the position of the ip header
            ih = reinterpret_cast<const gr_ip_header *>(packet + 14);  // length of ethernet header

            // retrieve the position of the udp header
            u_int ip_len;
            ip_len = (ih->ver_ihl & 0xf) * 4;
            uh = reinterpret_cast<const gr_udp_header *>(reinterpret_cast<const u_char *>(ih) + ip_len);

            // convert from network byte order to host byte order
            // u_short sport;
            u_short dport;
            dport = ntohs(uh->dport);
            // sport = ntohs(uh->sport);
            if (dport == d_udp_port)
                {
                    // print ip addresses and udp ports
                    //            printf("%d.%d.%d.%d.%d -> %d.%d.%d.%d.%d\n",
                    //                   ih->saddr.byte1,
                    //                   ih->saddr.byte2,
                    //                   ih->saddr.byte3,
                    //                   ih->saddr.byte4,
                    //                   sport,
                    //                   ih->daddr.byte1,
                    //                   ih->daddr.byte2,
                    //                   ih->daddr.byte3,
                    //                   ih->daddr.byte4,
                    //                   dport);
                    //            std::cout<<"uh->len:"<<ntohs(uh->len)<< '\n';

                    int payload_length_bytes = ntohs(uh->len) - 8;  // total udp packet length minus the header length
                    // read the payload bytes and insert them into the shared circular buffer
                    const u_char *udp_payload = (reinterpret_cast<const u_char *>(uh) + sizeof(gr_udp_header));
                    if (fifo_items <= (FIFO_SIZE - payload_length_bytes))
                        {
                            int aligned_write_items = FIFO_SIZE - fifo_write_ptr;
                            if (aligned_write_items >= payload_length_bytes)
                                {
                                    // write all in a single memcpy
                                    memcpy(&fifo_buff[fifo_write_ptr], &udp_payload[0], payload_length_bytes);  // size in bytes
                                    fifo_write_ptr += payload_length_bytes;
                                    if (fifo_write_ptr == FIFO_SIZE)
                                        {
                                            fifo_write_ptr = 0;
                                        }
                                    fifo_items += payload_length_bytes;
                                }
                            else
                                {
                                    // two step wrap write
                                    memcpy(&fifo_buff[fifo_write_ptr], &udp_payload[0], aligned_write_items);  // size in bytes
                                    fifo_write_ptr = payload_length_bytes - aligned_write_items;
                                    memcpy(&fifo_buff[0], &udp_payload[aligned_write_items], fifo_write_ptr);  // size in bytes
                                    fifo_items += payload_length_bytes;
                                }
                        }
                    else
                        {
                            // notify overflow
                            std::cout << "O" << std::flush;
                        }
                }
        }
}


void Gr_Complex_Ip_Packet_Source::my_pcap_loop_thread(pcap_t *pcap_handle)
{
    pcap_loop(pcap_handle, -1, Gr_Complex_Ip_Packet_Source::static_pcap_callback, reinterpret_cast<u_char *>(this));
}


void Gr_Complex_Ip_Packet_Source::demux_samples(const gr_vector_void_star &output_items, int num_samples_readed)
{
    for (int n = 0; n < num_samples_readed; n++)
        {
            switch (d_wire_sample_type)
                {
                case 1:  // interleaved byte samples
                    for (auto &output_item : output_items)
                        {
                            int8_t real;
                            int8_t imag;
                            real = fifo_buff[fifo_read_ptr++];
                            imag = fifo_buff[fifo_read_ptr++];
                            if (d_IQ_swap)
                                {
                                    static_cast<gr_complex *>(output_item)[n] = gr_complex(real, imag);
                                }
                            else
                                {
                                    static_cast<gr_complex *>(output_item)[n] = gr_complex(imag, real);
                                }
                        }
                    break;
                case 2:  // 4-bit samples
                    for (auto &output_item : output_items)
                        {
                            int8_t real;
                            int8_t imag;
                            uint8_t tmp_char2;
                            tmp_char2 = fifo_buff[fifo_read_ptr] & 0x0F;
                            if (tmp_char2 >= 8)
                                {
                                    real = 2 * (tmp_char2 - 16) + 1;
                                }
                            else
                                {
                                    real = 2 * tmp_char2 + 1;
                                }
                            tmp_char2 = fifo_buff[fifo_read_ptr++] >> 4;
                            tmp_char2 = tmp_char2 & 0x0F;
                            if (tmp_char2 >= 8)
                                {
                                    imag = 2 * (tmp_char2 - 16) + 1;
                                }
                            else
                                {
                                    imag = 2 * tmp_char2 + 1;
                                }
                            if (d_IQ_swap)
                                {
                                    static_cast<gr_complex *>(output_item)[n] = gr_complex(imag, real);
                                }
                            else
                                {
                                    static_cast<gr_complex *>(output_item)[n] = gr_complex(real, imag);
                                }
                        }
                    break;
                case 3:  // interleaved float samples
                    for (auto &output_item : output_items)
                        {
                            float real;
                            float imag;
                            memcpy(&real, &fifo_buff[fifo_read_ptr], sizeof(real));
                            fifo_read_ptr += 4;  // Four bytes in float
                            memcpy(&imag, &fifo_buff[fifo_read_ptr], sizeof(imag));
                            fifo_read_ptr += 4;  // Four bytes in float
                            if (d_IQ_swap)
                                {
                                    static_cast<gr_complex *>(output_item)[n] = gr_complex(real, imag);
                                }
                            else
                                {
                                    static_cast<gr_complex *>(output_item)[n] = gr_complex(imag, real);
                                }
                        }
                    break;
                default:
                    std::cout << "Unknown wire sample type\n";
                    exit(0);
                }
            if (fifo_read_ptr == FIFO_SIZE)
                {
                    fifo_read_ptr = 0;
                }
        }
}


int Gr_Complex_Ip_Packet_Source::work(int noutput_items,
    __attribute__((unused)) gr_vector_const_void_star &input_items,
    gr_vector_void_star &output_items)
{
    // send samples to next GNU Radio block
    boost::mutex::scoped_lock lock(d_mutex);  // hold mutex for duration of this function
    if (fifo_items == 0)
        {
            return 0;
        }

    if (output_items.size() > static_cast<uint64_t>(d_n_baseband_channels))
        {
            std::cout << "Configuration error: more baseband channels connected than the available in the UDP source\n";
            exit(0);
        }
    int num_samples_readed;
    int bytes_requested;
    switch (d_wire_sample_type)
        {
        case 1:  // complex byte samples
        case 2:  // complex 4 bits samples
        case 3:  // complex float samples
            bytes_requested = noutput_items * d_bytes_per_sample;
            if (bytes_requested < fifo_items)
                {
                    num_samples_readed = noutput_items;  // read all
                }
            else
                {
                    num_samples_readed = fifo_items / d_bytes_per_sample;  // read what we have
                }
            break;
        default:  // complex byte samples
            bytes_requested = noutput_items * d_bytes_per_sample;
            if (bytes_requested < fifo_items)
                {
                    num_samples_readed = noutput_items;  // read all
                }
            else
                {
                    num_samples_readed = fifo_items / d_bytes_per_sample;  // read what we have
                }
        }

    bytes_requested = num_samples_readed * d_bytes_per_sample;
    // read all in a single loop
    demux_samples(output_items, num_samples_readed);  // it also increases the fifo read pointer
    // update fifo items
    fifo_items = fifo_items - bytes_requested;

    for (uint64_t n = 0; n < output_items.size(); n++)
        {
            produce(static_cast<int>(n), num_samples_readed);
        }
    return this->WORK_CALLED_PRODUCE;
}
