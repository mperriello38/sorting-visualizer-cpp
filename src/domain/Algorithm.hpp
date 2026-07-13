#pragma once

// Shared algorithm vocabulary used by app configuration and sorting dispatch.
// Keeping it in domain prevents either layer from owning a duplicate definition.
enum class Algorithm {
    Selection,
    Bubble,
    Insertion
};
