-- Minima Analytics — DuckDB schema (v1)
-- Conventions:
--   * hashes/ids/addresses/token ids are canonical 0x-lowercase VARCHAR
--   * amounts are raw integer units; display scale lives in analytics.config
--   * height is authoritative; timestamps are advisory (UTC ms)
--   * archive remains the source of truth for raw blocks; this DB is derived
CREATE SCHEMA IF NOT EXISTS analytics;
USE analytics;

-- ---------------------------------------------------------------------------
-- Configuration and metric catalogue
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS config (
    key    VARCHAR PRIMARY KEY,
    value  VARCHAR NOT NULL
);
INSERT INTO config VALUES
    ('schema_version',      '1'),
    ('amount_scale',        '4'),
    ('native_token_id',     '0x00'),
    ('finality_depth',      '2048'),
    ('stale_after_blocks',  '12'),
    ('age_bands',           '["0-1d","1d-1w","1w-1m","1m-3m","3m-6m","6m-1y","1y-2y","2y-5y","5y+"]'),
    ('value_bands',         '["0-1","1-10","10-100","100-1k","1k-10k","10k-100k","100k-1m","1m+"]')
ON CONFLICT (key) DO NOTHING;

CREATE TABLE IF NOT EXISTS metric_catalog (
    metric       VARCHAR PRIMARY KEY,
    unit         VARCHAR NOT NULL,
    granularity  VARCHAR NOT NULL,
    source       VARCHAR NOT NULL CHECK (source IN ('archive','megammr','live','index','derived')),
    min_mode     VARCHAR NOT NULL CHECK (min_mode IN ('full','archive','megammr','analytics')),
    description  VARCHAR NOT NULL
);
INSERT INTO metric_catalog VALUES
 ('block_height','count','1m/1h/1d','live','full','Max block height at bucket end'),
 ('block_count','count','1m/1h/1d','archive','archive','Blocks in bucket'),
 ('tx_count','count','1m/1h/1d','archive','archive','Transactions in bucket'),
 ('tx_rate','tx/s','1m/1h','archive','archive','Transactions per second'),
 ('fees_total_raw','raw','1h/1d','archive','archive','Sum of transaction fees'),
 ('coinbase_reward_raw','raw','1d','archive','archive','Sum of coinbase outputs'),
 ('output_volume_raw','raw','1h/1d','archive','archive','Sum of non-coinbase outputs'),
 ('input_volume_raw','raw','1h/1d','archive','archive','Sum of spent inputs'),
 ('active_addresses','count','1h/1d','archive','archive','Unique input or output addresses'),
 ('new_addresses','count','1d','archive','archive','First-seen output addresses'),
 ('sender_addresses','count','1d','archive','archive','Unique input addresses'),
 ('receiver_addresses','count','1d','archive','archive','Unique output addresses'),
 ('utxo_count','count','snapshot','megammr','megammr','Unspent coins at as_of'),
 ('supply_raw','raw','snapshot','megammr','megammr','Unspent native supply at as_of'),
 ('spent_coin_count','count','1d','archive','archive','Coins first spent in bucket'),
 ('utxo_age_band_count','count','snapshot','archive','archive','Unspent coins per age band'),
 ('utxo_age_band_value_raw','raw','snapshot','archive','archive','Value per age band'),
 ('utxo_value_band_count','count','snapshot','megammr','megammr','Coins per value band'),
 ('holder_cohort_count','count','snapshot','megammr','megammr','Addresses per balance cohort'),
 ('holder_cohort_value_raw','raw','snapshot','megammr','megammr','Balance per cohort'),
 ('address_inflow_raw','raw','1d','archive','archive','Value received by address'),
 ('address_outflow_raw','raw','1d','archive','archive','Value spent by address'),
 ('blocks_mined','count','1d','archive','archive','Blocks by minter'),
 ('index_lag_blocks','count','1m','index','analytics','Live tip minus last indexed height'),
 ('archive_mmr_divergence','count','snapshot','index','analytics','UTXO mismatches at check height'),
 ('reorg_depth','count','event','live','full','Deepest recent reorg depth')
ON CONFLICT (metric) DO NOTHING;

-- ---------------------------------------------------------------------------
-- Raw-ish indexed chain data (derived from the archive)
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS chain_blocks (
    height          BIGINT PRIMARY KEY,
    hash            VARCHAR NOT NULL UNIQUE,
    prev_hash       VARCHAR,
    timestamp_ms    BIGINT NOT NULL,
    tx_count        INTEGER NOT NULL DEFAULT 0,
    size_bytes      INTEGER,
    minter_address  VARCHAR,
    reward_raw      HUGEINT,
    fees_raw        HUGEINT,
    finalized       BOOLEAN NOT NULL DEFAULT TRUE
);

CREATE TABLE IF NOT EXISTS txs (
    txpowid        VARCHAR PRIMARY KEY,
    height         BIGINT NOT NULL,
    tx_index       INTEGER NOT NULL,
    timestamp_ms   BIGINT NOT NULL,
    is_coinbase    BOOLEAN NOT NULL DEFAULT FALSE,
    fee_raw        HUGEINT,
    input_count    INTEGER NOT NULL DEFAULT 0,
    output_count   INTEGER NOT NULL DEFAULT 0,
    size_bytes     INTEGER,
    token_id       VARCHAR,
    burn_raw       HUGEINT,
    UNIQUE (height, tx_index)
);

CREATE TABLE IF NOT EXISTS tx_inputs (
    txpowid        VARCHAR NOT NULL,
    input_index    INTEGER NOT NULL,
    coin_id        VARCHAR NOT NULL,
    address        VARCHAR,
    token_id       VARCHAR,
    amount_raw     HUGEINT NOT NULL,
    created_height BIGINT,
    PRIMARY KEY (txpowid, input_index)
);

CREATE TABLE IF NOT EXISTS tx_outputs (
    txpowid         VARCHAR NOT NULL,
    output_index    INTEGER NOT NULL,
    coin_id         VARCHAR NOT NULL UNIQUE,
    address         VARCHAR,
    token_id        VARCHAR,
    amount_raw      HUGEINT NOT NULL,
    script          VARCHAR,
    state           VARCHAR,
    created_height  BIGINT NOT NULL,
    spent_height    BIGINT,
    spent_txid      VARCHAR,
    PRIMARY KEY (txpowid, output_index)
);

CREATE TABLE IF NOT EXISTS coins (
    coin_id         VARCHAR PRIMARY KEY,
    address         VARCHAR,
    token_id        VARCHAR,
    amount_raw      HUGEINT NOT NULL,
    created_height  BIGINT NOT NULL,
    created_ms      BIGINT,
    spent_height    BIGINT,
    spent_ms        BIGINT,
    spent_txid      VARCHAR,
    script          VARCHAR,
    state           VARCHAR,
    is_spent        BOOLEAN NOT NULL DEFAULT FALSE,
    mmr_entry       VARCHAR
);

CREATE TABLE IF NOT EXISTS addresses (
    address            VARCHAR PRIMARY KEY,
    first_seen_height  BIGINT,
    first_seen_ms      BIGINT,
    last_seen_height   BIGINT,
    last_seen_ms       BIGINT,
    tx_count           BIGINT NOT NULL DEFAULT 0,
    label              VARCHAR
);

CREATE TABLE IF NOT EXISTS token_registry (
    token_id          VARCHAR PRIMARY KEY,
    name              VARCHAR,
    symbol            VARCHAR,
    decimals          INTEGER,
    total_supply_raw  HUGEINT,
    first_seen_height BIGINT
);

-- ---------------------------------------------------------------------------
-- Snapshots (MMR and archive replay)
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS utxo_snapshots (
    height        BIGINT PRIMARY KEY,
    block_hash    VARCHAR NOT NULL,
    mmr_root      VARCHAR,
    coin_count    BIGINT NOT NULL,
    total_raw     HUGEINT NOT NULL,
    created_at_ms BIGINT NOT NULL
);

CREATE TABLE IF NOT EXISTS address_balances (
    snapshot_height BIGINT NOT NULL,
    address         VARCHAR NOT NULL,
    balance_raw     HUGEINT NOT NULL,
    coin_count      INTEGER NOT NULL,
    PRIMARY KEY (snapshot_height, address)
);

CREATE TABLE IF NOT EXISTS utxo_age_bands (
    snapshot_height BIGINT NOT NULL,
    band            VARCHAR NOT NULL,
    coin_count      BIGINT NOT NULL,
    amount_raw      HUGEINT NOT NULL,
    PRIMARY KEY (snapshot_height, band)
);

CREATE TABLE IF NOT EXISTS utxo_value_bands (
    snapshot_height BIGINT NOT NULL,
    band            VARCHAR NOT NULL,
    coin_count      BIGINT NOT NULL,
    amount_raw      HUGEINT NOT NULL,
    PRIMARY KEY (snapshot_height, band)
);

CREATE TABLE IF NOT EXISTS holder_cohorts (
    snapshot_height BIGINT NOT NULL,
    cohort          VARCHAR NOT NULL,
    address_count   BIGINT NOT NULL,
    balance_raw     HUGEINT NOT NULL,
    PRIMARY KEY (snapshot_height, cohort)
);

-- ---------------------------------------------------------------------------
-- Derived time series and index health
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS metric_series (
    metric          VARCHAR NOT NULL,
    granularity     VARCHAR NOT NULL,
    bucket_start_ms BIGINT NOT NULL,
    height          BIGINT,
    value           DOUBLE,
    value_raw       HUGEINT,
    source          VARCHAR NOT NULL,
    complete        BOOLEAN NOT NULL DEFAULT TRUE,
    PRIMARY KEY (metric, granularity, bucket_start_ms)
);

CREATE TABLE IF NOT EXISTS consistency_checks (
    height                BIGINT PRIMARY KEY,
    block_hash            VARCHAR,
    archive_root          VARCHAR,
    mmr_root              VARCHAR,
    coin_count_archive    BIGINT,
    coin_count_mmr        BIGINT,
    matched               BOOLEAN NOT NULL,
    checked_at_ms         BIGINT NOT NULL,
    details               VARCHAR
);

CREATE TABLE IF NOT EXISTS reorg_events (
    id              VARCHAR PRIMARY KEY,
    height          BIGINT NOT NULL,
    old_hash        VARCHAR,
    new_hash        VARCHAR,
    depth           INTEGER NOT NULL,
    detected_at_ms  BIGINT NOT NULL
);

CREATE TABLE IF NOT EXISTS ingest_blocks (
    height         BIGINT PRIMARY KEY,
    hash           VARCHAR NOT NULL,
    status         VARCHAR NOT NULL CHECK (status IN ('ok','partial','reorg')),
    ingested_at_ms BIGINT NOT NULL
);

CREATE TABLE IF NOT EXISTS index_state (
    id                INTEGER PRIMARY KEY CHECK (id = 1),
    schema_version    INTEGER NOT NULL,
    last_height       BIGINT,
    last_hash         VARCHAR,
    archive_from      BIGINT,
    archive_to        BIGINT,
    tx_detail_from    BIGINT,
    mmr_as_of         BIGINT,
    mmr_hash          VARCHAR,
    lag_blocks        BIGINT,
    status            VARCHAR NOT NULL CHECK (status IN ('building','ready','stale','degraded')),
    updated_at_ms     BIGINT NOT NULL
);
INSERT INTO index_state VALUES (1, 1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                                'building', 0)
ON CONFLICT (id) DO NOTHING;

-- Optional v2 enrichment
CREATE TABLE IF NOT EXISTS entities (
    entity_id   VARCHAR PRIMARY KEY,
    label       VARCHAR,
    category    VARCHAR,
    source      VARCHAR,
    updated_ms  BIGINT
);

CREATE TABLE IF NOT EXISTS entity_addresses (
    entity_id  VARCHAR NOT NULL,
    address    VARCHAR NOT NULL,
    confidence DOUBLE,
    PRIMARY KEY (entity_id, address)
);

-- ---------------------------------------------------------------------------
-- Indexes
-- ---------------------------------------------------------------------------
CREATE INDEX IF NOT EXISTS idx_txs_height        ON txs(height);
CREATE INDEX IF NOT EXISTS idx_txs_timestamp     ON txs(timestamp_ms);
CREATE INDEX IF NOT EXISTS idx_inputs_coin       ON tx_inputs(coin_id);
CREATE INDEX IF NOT EXISTS idx_inputs_address    ON tx_inputs(address);
CREATE INDEX IF NOT EXISTS idx_outputs_address   ON tx_outputs(address);
CREATE INDEX IF NOT EXISTS idx_outputs_spent     ON tx_outputs(spent_height);
CREATE INDEX IF NOT EXISTS idx_coins_address     ON coins(address);
CREATE INDEX IF NOT EXISTS idx_coins_token       ON coins(token_id);
CREATE INDEX IF NOT EXISTS idx_coins_spent       ON coins(spent_height);
CREATE INDEX IF NOT EXISTS idx_metric_series     ON metric_series(metric, granularity, bucket_start_ms);

-- ---------------------------------------------------------------------------
-- Convenience views
-- ---------------------------------------------------------------------------
CREATE OR REPLACE VIEW v_latest_utxo_snapshot AS
SELECT * FROM utxo_snapshots ORDER BY height DESC LIMIT 1;

CREATE OR REPLACE VIEW v_supply AS
SELECT
    (SELECT value FROM config WHERE key = 'native_token_id') AS token_id,
    COALESCE(SUM(amount_raw), 0) AS supply_raw,
    COUNT(*) AS coin_count
FROM coins
WHERE is_spent = FALSE
  AND token_id = (SELECT value FROM config WHERE key = 'native_token_id');

CREATE OR REPLACE VIEW v_latest_address_balances AS
SELECT ab.*
FROM address_balances ab
WHERE ab.snapshot_height = (SELECT MAX(snapshot_height) FROM address_balances);

CREATE OR REPLACE VIEW v_latest_age_bands AS
SELECT * FROM utxo_age_bands
WHERE snapshot_height = (SELECT MAX(snapshot_height) FROM utxo_age_bands);

CREATE OR REPLACE VIEW v_latest_value_bands AS
SELECT * FROM utxo_value_bands
WHERE snapshot_height = (SELECT MAX(snapshot_height) FROM utxo_value_bands);

CREATE OR REPLACE VIEW v_coverage AS
SELECT
    (SELECT archive_from FROM index_state WHERE id = 1) AS archive_from,
    (SELECT archive_to FROM index_state WHERE id = 1) AS archive_to,
    (SELECT tx_detail_from FROM index_state WHERE id = 1) AS tx_detail_from,
    (SELECT mmr_as_of FROM index_state WHERE id = 1) AS mmr_as_of,
    (SELECT last_height FROM index_state WHERE id = 1) AS last_height,
    (SELECT lag_blocks FROM index_state WHERE id = 1) AS lag_blocks,
    (SELECT status FROM index_state WHERE id = 1) AS status;
