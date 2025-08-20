import Bonjour from "bonjour-service";

let bonjourService: Bonjour | null = null;
let publishedService: any = null;

export const start_MDNS = (MQTT_PORT: number, HOST_IP: string = "10.0.0.161") => {
    try {
        // Create new bonjour instance if not exists
        if (!bonjourService) {
            bonjourService = new Bonjour();
        }
        
        // Unpublish existing service if any
        if (publishedService) {
            publishedService.stop();
        }
        
        const name = "Local MQTT Controller";
        publishedService = bonjourService.publish({ 
            name: name, 
            type: "mqtt", 
            port: MQTT_PORT,
            host: HOST_IP, // Add the IP address to the service record
            txt: { 
                path: "/",
                version: "1.0",
                protocol: "mqtt",
                ip: HOST_IP // Also include IP in TXT record for redundancy
            } 
        });
        
        console.log(`[mdns] Published service: ${name} on ${HOST_IP}:${MQTT_PORT}`);
        
        // Handle service events
        publishedService.on('up', () => {
            console.log(`[mdns] Service ${name} is now available on network at ${HOST_IP}:${MQTT_PORT}`);
        });
        
        publishedService.on('error', (err: any) => {
            console.error(`[mdns] Service error:`, err);
        });
        
    } catch (error) {
        console.error("[mdns] Failed to start MDNS service:", error);
    }
}

export const check_MDNS_Service = (MQTT_PORT: number, HOST_IP: string = "10.0.0.161", timer: number) => {
    setInterval(() => {
        if (!bonjourService || !publishedService) {
            console.log("[mdns] Service missing, restarting...");
            start_MDNS(MQTT_PORT, HOST_IP);
        }
    }, timer);
};

export const stop_MDNS = () => {
    if (publishedService) {
        publishedService.stop();
        publishedService = null;
    }
    if (bonjourService) {
        bonjourService.destroy();
        bonjourService = null;
    }
    console.log("[mdns] Service stopped");
};
