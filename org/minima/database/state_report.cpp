#include "org/minima/database/state_report.hpp"

#include <string>
#include <vector>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/archive/archive_manager.hpp"
#include "org/minima/database/cascade/cascade.hpp"
#include "org/minima/database/cascade/cascade_node.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/mmr/mega_m_m_r.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/utils/minima_logger.hpp"

namespace org {
namespace minima {
namespace database {

using org::minima::utils::json::JSONObject;

void StateReport::build(JSONObject& out) {
    using org::minima::system::params::GeneralParams;

    bool ok = true;
    std::string issues;
    auto addIssue = [&](const std::string& zIssue) {
        ok = false;
        if (!issues.empty()) issues += "; ";
        issues += zIssue;
    };

#ifdef TOTEM_NODE_VERSION
    out.put("version", std::string(TOTEM_NODE_VERSION));
#else
    out.put("version", std::string("unknown"));
#endif
    out.put("ok", false);
    out.put("issues", std::string());

    MinimaDB* db = MinimaDB::getDB();
    if (db == nullptr) {
        out.put("issues", std::string("no database"));
        return;
    }

    // ---- Archive blocks -------------------------------------------------
    long long archive_first = -1;
    long long archive_last = -1;
    long long archive_rows = 0;
    bool archive_contiguous = false;

    try {
        auto first = db->getArchive().loadFirstBlock();
        auto last = db->getArchive().loadLastBlock();
        if (first) archive_first = first->getTxPoW().getBlockNumber().getAsLong();
        if (last) archive_last = last->getTxPoW().getBlockNumber().getAsLong();
        archive_rows = db->getArchive().getSize();

        if (archive_first >= 0 && archive_last >= archive_first) {
            archive_contiguous =
                (archive_rows == (archive_last - archive_first + 1));
            if (!archive_contiguous) {
                addIssue("archive is not contiguous: rows " + std::to_string(archive_rows) +
                         " for " + std::to_string(archive_first) + ".." + std::to_string(archive_last));
            }
        } else {
            addIssue("archive is empty");
        }
    } catch (const std::exception& e) {
        addIssue(std::string("archive read failed: ") + e.what());
    }

    // ---- Cascade --------------------------------------------------------
    long long cascade_tip = -1;
    long long cascade_length = 0;
    bool cascade_valid = false;

    try {
        auto& cascade = db->getCascade();
        cascade_length = cascade.getLength();
        auto tip = cascade.getTip();
        if (!tip) {
            addIssue("cascade is empty");
        } else {
            cascade_tip = tip->getTxPoW().getBlockNumber().getAsLong();
            cascade_valid = org::minima::database::cascade::Cascade::checkCascadeCorrect(cascade);
            if (!cascade_valid) addIssue("cascade failed checkCascadeCorrect");
        }
    } catch (const std::exception& e) {
        addIssue(std::string("cascade read failed: ") + e.what());
    }

    long long gap_blocks = 0;
    if (cascade_tip >= 0 && archive_last >= 0) {
        gap_blocks = cascade_tip - archive_last;
        if (gap_blocks > 0) {
            addIssue("archive gap: cascade tip " + std::to_string(cascade_tip) +
                     " is ahead of archive last " + std::to_string(archive_last) +
                     " (run 'archive action:replay')");
        } else if (archive_first >= 0 && cascade_tip < archive_first) {
            addIssue("cascade tip " + std::to_string(cascade_tip) +
                     " is below archive first " + std::to_string(archive_first));
        }
    }

    // ---- TxPoWTree ------------------------------------------------------
    long long tree_root = -1;
    long long tree_tip = -1;
    long long tree_size = 0;

    try {
        auto& tree = db->getTxPoWTree();
        tree_size = tree.getSize();
        auto root = tree.getRoot();
        auto tip = tree.getTip();
        if (root) tree_root = root->getTxPoW().getBlockNumber().getAsLong();
        if (tip) tree_tip = tip->getTxPoW().getBlockNumber().getAsLong();

        if (root && cascade_tip >= 0 && tree_root != cascade_tip + 1) {
            addIssue("tree root " + std::to_string(tree_root) +
                     " != cascade tip + 1 (" + std::to_string(cascade_tip + 1) + ")");
        }
        if (tip && cascade_tip >= 0 && tree_tip < cascade_tip) {
            addIssue("tree tip " + std::to_string(tree_tip) +
                     " is below cascade tip " + std::to_string(cascade_tip));
        }
    } catch (const std::exception& e) {
        addIssue(std::string("tree read failed: ") + e.what());
    }

    // ---- MegaMMR --------------------------------------------------------
    long long mmr_coins = 0;

    try {
        if (GeneralParams::IS_MEGAMMR) {
            mmr_coins = static_cast<long long>(db->getMegaMMR().coinCount());
        }
    } catch (const std::exception& e) {
        addIssue(std::string("MMR read failed: ") + e.what());
    }

    out.put("ok", ok);
    out.put("issues", issues);
    out.put("archive_first", archive_first);
    out.put("archive_last", archive_last);
    out.put("archive_rows", archive_rows);
    out.put("archive_contiguous", archive_contiguous);
    out.put("cascade_tip", cascade_tip);
    out.put("cascade_length", cascade_length);
    out.put("cascade_valid", cascade_valid);
    out.put("tree_root", tree_root);
    out.put("tree_tip", tree_tip);
    out.put("tree_size", tree_size);
    out.put("gap_blocks", gap_blocks);
    out.put("mmr_enabled", GeneralParams::IS_MEGAMMR);
    out.put("mmr_coins", mmr_coins);
}

} // namespace database
} // namespace minima
} // namespace org
