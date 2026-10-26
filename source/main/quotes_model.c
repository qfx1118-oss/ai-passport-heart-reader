#include "quotes_model.h"
void quotes_model_init(quotes_model_t *m, size_t count, uint64_t now) {
    *m = (quotes_model_t){.count=count, .automatic=count > 1, .changed_ms=now};
}
void quotes_model_step(quotes_model_t *m, int direction, uint64_t now) {
    if (m->count) {
        m->index = direction < 0 ? (m->index + m->count - 1) % m->count
                                 : (m->index + 1) % m->count;
    }
    m->changed_ms = now;
}
void quotes_model_toggle(quotes_model_t *m, uint64_t now) {
    m->automatic = m->count > 1 && !m->automatic;
    m->changed_ms = now;
}
bool quotes_model_tick(quotes_model_t *m, uint64_t now) {
    if (!m->automatic || m->count < 2 || now - m->changed_ms < QUOTE_INTERVAL_MS) return false;
    /* Advance once after a stall: always give the next quote a full dwell. */
    quotes_model_step(m, 1, now);
    return true;
}
unsigned quotes_model_remaining(const quotes_model_t *m, uint64_t now) {
    uint64_t elapsed = now - m->changed_ms;
    return elapsed >= QUOTE_INTERVAL_MS ? 0 : (unsigned)((QUOTE_INTERVAL_MS - elapsed + 999) / 1000);
}
