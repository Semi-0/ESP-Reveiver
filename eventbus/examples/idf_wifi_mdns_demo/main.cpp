#include "eventbus/TinyEventBus.h"
#include "eventbus/EventProtocol.h"
#include "eventbus/FlowGraph.h"
#include <stdio.h>
#include <string.h>

// --- Mock WiFi / mDNS glue (replace with real implementations) ---
static bool mdns_query_worker(void** out) {
    const char* host = "device-1.local";
    *out = strdup(host);        // caller will later free or ignore
    // return false on failure
    return true;
}

// Global bus
static TinyEventBus BUS;

extern "C" void app_main(void) {
    BUS.begin();

    // (Pretend WiFi connects here… then:)
    BUS.publish(Event{TOPIC_WIFI_CONNECTED, 0, nullptr});

    // Build declarative flows
    FlowGraph G(BUS);

    // when WiFi connects: do mDNS lookup; publish result topic
    G.when(TOPIC_WIFI_CONNECTED,
        G.async_blocking("mdns-q", mdns_query_worker,
            // onOk → publish MDNS_FOUND with e.ptr = hostname
            FlowGraph::publish(TOPIC_MDNS_FOUND),
            // onErr → publish MDNS_FAILED
            FlowGraph::publish(TOPIC_MDNS_FAILED)
        )
    );

    // Fan-out: also log when WiFi connects (example tee/seq)
    G.when(TOPIC_WIFI_CONNECTED,
        FlowGraph::publish(TOPIC_MDNS_FAILED, -1) // sentinel log event
    );

    // Listener: print MDNS_FOUND
    BUS.subscribe([](const Event& e, void*){
        const char* host = static_cast<const char*>(e.ptr);
        printf("[APP] mDNS found: %s\n", host ? host : "<null>");
        // If you allocated the payload, free it when you're done:
        if (host) free((void*)host);
    }, nullptr, bit(TOPIC_MDNS_FOUND));
}
