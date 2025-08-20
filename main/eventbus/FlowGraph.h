#pragma once
#include <functional>
#include <array>
#include "eventbus/EventBus.h"
#include "eventbus/EventProtocol.h"
#include "freertos/task.h"

// A Flow is a small callable
using Flow = std::function<void(const Event&, IEventBus&)>;

class FlowGraph {
public:
    explicit FlowGraph(IEventBus& bus) : bus_(bus) {
        asyncRouter_ = bus_.subscribe(&FlowGraph::asyncRouterHandler, this, bit(TOPIC__ASYNC_RESULT));
    }
    ~FlowGraph() {
        // Unsubscribe router + user subscriptions, free heap-stored Flows
        if (asyncRouter_ >= 0) bus_.unsubscribe(asyncRouter_);
        for (int i=0;i<count_;++i) {
            bus_.unsubscribe(subs_[i].h);
            delete subs_[i].flow; // delete heap Flow
        }
    }

    // Declarative "when(topic, flow)"
    ListenerHandle when(uint16_t topic, const Flow& f) {
        if (count_ >= MAX_SUBS) return -1;
        Flow* stored = new Flow(f);
        auto h = bus_.subscribe(
            [](const Event& e, void* u) {
                // Placeholder - this will be replaced below
                (void)e; (void)u;
            }, this, bit(topic)
        );
        // We can't pass Flow directly to handler above; rebind a proper handler:
        bus_.unsubscribe(h);
        h = bus_.subscribe(
            [](const Event& e, void* u) {
                auto* pair = static_cast<UserPair*>(u);
                (*pair->flow)(e, *pair->fg->busPtr());
            },
            new UserPair{this, stored},
            bit(topic)
        );
        subs_[count_++] = {h, stored};
        return h;
    }

    // Operators
    static Flow publish(uint16_t topic, int32_t i32=0, void* ptr=nullptr) {
        return [=](const Event&, IEventBus& bus){ bus.publish(Event{topic, i32, ptr}); };
    }

    static Flow seq(const Flow& a, const Flow& b) {
        return [=](const Event& e, IEventBus& bus){ a(e,bus); b(e,bus); };
    }

    static Flow tee(const Flow& a, const Flow& b) { return seq(a,b); }

    static Flow filter(std::function<bool(const Event&)> pred, const Flow& thenF) {
        return [=](const Event& e, IEventBus& bus){ if (pred(e)) thenF(e,bus); };
    }

    static Flow branch(std::function<bool(const Event&)> pred,
                       const Flow& onT, const Flow& onF) {
        return [=](const Event& e, IEventBus& bus){ (pred(e)?onT:onF)(e,bus); };
    }

    // Tap operator - observe/log without publishing
    static Flow tap(std::function<void(const Event&)> fn) {
        return [=](const Event& e, IEventBus&) {
            fn(e); // only observe/log, never publish
        };
    }

    // Async: run workerFn in a FreeRTOS task and resume flow with payload via TOPIC__ASYNC_RESULT
    // Task pack structure for async operations
    struct TaskPack { 
        std::function<bool(void**)> fn; 
        void* ctx;  // Changed to void* to avoid circular dependency
    };

    Flow async_blocking(const char* name,
                        std::function<bool(void** out)> workerFn,
                        const Flow& onOk,
                        const Flow& onErr,
                        uint32_t stack=4096,
                        UBaseType_t prio=tskIDLE_PRIORITY+1) {
        return [=, this](const Event& trigger, IEventBus& /*bus*/) {
            // Pack everything for continuation
            struct AsyncCtx { Flow onOk; Flow onErr; Event original; FlowGraph* fg; };
            auto* ctx = new AsyncCtx{onOk, onErr, trigger, const_cast<FlowGraph*>(this)};
            xTaskCreate([](void* arg){
                auto* p = static_cast<TaskPack*>(arg);
                void* payload=nullptr;
                bool ok = p->fn(&payload);
                // Post result back into bus via synthetic event:
                auto* async_ctx = static_cast<AsyncCtx*>(p->ctx);
                Event r{TOPIC__ASYNC_RESULT, ok?1:0, new ResultPack{payload, p->ctx}};
                async_ctx->fg->bus_.publish(r);
                delete p;
                vTaskDelete(nullptr);
            }, name, stack, new TaskPack{workerFn, ctx}, prio, nullptr);
        };
    }
    
    // New version that passes event data to worker function
    Flow async_blocking_with_event(const char* name,
                        std::function<bool(const Event& trigger_event, void** out)> workerFn,
                        const Flow& onOk,
                        const Flow& onErr,
                        uint32_t stack=4096,
                        UBaseType_t prio=tskIDLE_PRIORITY+1) {
        return [=, this](const Event& trigger, IEventBus& /*bus*/) {
            // Pack everything for continuation including the trigger event
            struct AsyncCtx { Flow onOk; Flow onErr; Event original; FlowGraph* fg; };
            struct TaskPackWithEvent { std::function<bool(const Event&, void**)> fn; void* ctx; Event trigger_event; };
            auto* ctx = new AsyncCtx{onOk, onErr, trigger, const_cast<FlowGraph*>(this)};
            xTaskCreate([](void* arg){
                auto* p = static_cast<TaskPackWithEvent*>(arg);
                void* payload=nullptr;
                bool ok = p->fn(p->trigger_event, &payload);
                // Post result back into bus via synthetic event:
                auto* async_ctx = static_cast<AsyncCtx*>(p->ctx);
                Event r{TOPIC__ASYNC_RESULT, ok?1:0, new ResultPack{payload, p->ctx}};
                async_ctx->fg->bus_.publish(r);
                delete p;
                vTaskDelete(nullptr);
            }, name, stack, new TaskPackWithEvent{workerFn, ctx, trigger}, prio, nullptr);
        };
    }

private:
    // Continuation payload
    struct ResultPack { void* userPayload; void* ctx; };

    struct SubRec { ListenerHandle h=-1; Flow* flow=nullptr; };
    static constexpr int MAX_SUBS = 16;

    // We pass both FlowGraph* and Flow* to handler
    struct UserPair { FlowGraph* fg; Flow* flow; };

    IEventBus& bus_;
    ListenerHandle asyncRouter_ = -1;
    std::array<SubRec, MAX_SUBS> subs_{};
    int count_ = 0;

    IEventBus* busPtr() { return &bus_; }

    static void asyncRouterHandler(const Event& e, void* selfVoid) {
        auto* self = static_cast<FlowGraph*>(selfVoid);
        auto* pack = static_cast<ResultPack*>(e.ptr);
        auto* ctx  = static_cast<AsyncCtx*>(pack->ctx);
        Event shadow = ctx->original;    // restore original trigger
        shadow.ptr   = pack->userPayload;// pass worker payload to next
        if (e.i32 == 1) ctx->onOk(shadow, self->bus_);
        else            ctx->onErr(shadow, self->bus_);
        delete ctx;
        delete pack;
    }

    // Mirror AsyncCtx here to satisfy the static handler
    struct AsyncCtx { Flow onOk; Flow onErr; Event original; FlowGraph* fg; };

    // Removed unused current_ member
};

