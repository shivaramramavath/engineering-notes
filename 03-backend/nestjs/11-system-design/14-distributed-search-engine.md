---
title: "System Design: Distributed Search and Autocomplete Engine"
description: "Designing an enterprise search system with Elasticsearch, PostgreSQL CDC, and Trie-based autocomplete"
tags: [nestjs, system-design, search, elasticsearch, cdc]
---

# System Design: Distributed Search and Autocomplete Engine

## Architecture
- **CDC Pipeline**: Debezium captures PostgreSQL row changes and synchronizes Elasticsearch indexes in real time.
- **Autocomplete**: In-memory Redis Prefix Trie or Elasticsearch Completion Suggester for $< 5\text{ms}$ suggestions.
