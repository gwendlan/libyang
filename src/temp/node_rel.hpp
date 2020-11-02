#ifndef NODE_REL_H_
#define NODE_REL_H_

#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <iterator>

extern "C"
{
#include "new.h"
}

/* Comment trt_tree_ctx struct definition in new.h */

using std::vector;
using std::string;

struct Node
{
    using childs_v = vector<string>;

    string name;
    childs_v childs;

    Node(string p_name, vector<string> p_childs):
        name(p_name), childs(p_childs){}
};
using childs_iter = Node::childs_v::iterator;

struct Attributes
{
    struct Data
    {
        trt_node_type node_type;
        string type_name;
        bool feature;
    };
    string key;
    Data data;
};

struct Tree
{
    using tree_t = std::map<string, vector<string>>;
    vector<Attributes> att_v;
    tree_t map;

    Tree(vector<Node> nodes)
    {
        for(auto const& node: nodes)
            map.insert({node.name, node.childs});
    }

    Tree(vector<Node> nodes, vector<Attributes> att):
        att_v(att)
    {
        for(auto const& node: nodes)
            map.insert({node.name, node.childs});
    }
};
using tree_iter = Tree::tree_t::iterator;

struct trt_tree_ctx
{
    Tree tree;
    tree_iter row;
    int64_t child_idx;
};

trt_node attributed_node(Attributes const& att)
{
    trt_type type = att.data.type_name.empty() ?
        (trt_type){trd_type_empty, ""} :
        (trt_type){trd_type_name, att.data.type_name.c_str()};
    return 
        {
            trd_status_type_current, trd_flags_type_rw,
            {att.data.node_type, "", att.key.c_str()},
            type,
            att.data.feature
        };
}

trt_node default_node(string const& name)
{
    return (trt_node)
    {
        trd_status_type_current, trd_flags_type_rw,
        {trd_node_else, "", name.c_str()},
        {trd_type_empty, ""},
        trp_empty_iffeature()
    };
}

trt_node get_node(string const& name, vector<Attributes> const& att)
{
    if(att.empty())
        return default_node(name);
    for(auto const& item: att) {
        if(item.key == name)
            return attributed_node(item);
    }
    return default_node(name);
}

trt_node node(const struct trt_tree_ctx *ctx)
{
    /* if pointing to parent */
    if(ctx->child_idx < 0) {
        /* get parent name */ 
        auto& node_name = ctx->row->first;
        /* return node */
        return get_node(node_name, ctx->tree.att_v);
    } else {
        /* get child name */ 
        auto& node_name = ctx->row->second[ctx->child_idx];
        /* return node */
        return get_node(node_name, ctx->tree.att_v);
    }
}

trt_node next_sibling(struct trt_tree_ctx *ctx)
{
    auto& child_idx = ctx->child_idx;
    /* if pointing to parent name */ 
    if(child_idx < 0) {
        /* find if has siblings */
        childs_iter citer;
        bool succ = false;
        auto& node_name = ctx->row->first;
        for(auto item = ctx->tree.map.begin(); item != ctx->tree.map.end(); ++item) {
            auto& childs = item->second;
            if((citer = std::find(childs.begin(), childs.end(), node_name)) != childs.end()) {
                /* he is already last sibling -> no sibling? */
                if(citer+1 == childs.end())
                    continue;
                succ = true;
                /* store location */
                ctx->row = item;
                break;
            }
        }

        if(succ) {
            /* siblings was founded */
            /* iterator to index */
            child_idx = std::distance(ctx->row->second.begin(), citer);
            /* continue to get sibling */
        } else {
            /* maybe it is root node */
            tree_iter iter;
            /* warning: root nodes must be ordered */
            for(iter = std::next(ctx->row); iter != ctx->tree.map.end(); ++iter) {
                if(iter->first.empty())
                    break;
                bool succ = true;
                /* check if node is root node -> node cannot be in vector */
                for(auto giter = ctx->tree.map.begin(); giter != ctx->tree.map.end(); ++giter)
                {
                    if((std::find(giter->second.begin(), giter->second.end(), iter->first)) != giter->second.end()) {
                        /* iter->first is not root node, continue */
                        succ = false;
                        break;
                    }
                }
                if(succ) {
                    auto& node_name = iter->first;
                    ctx->row = iter;
                    child_idx = -1;
                    return get_node(node_name, ctx->tree.att_v);
                }
            }
            /* no sibling */
            return trp_empty_node();
        }
    }
    /* else we are already pointing to siblings */ 

    /* if there is no sibling */ 
    if(child_idx + 1 >= static_cast<int64_t>(ctx->row->second.size())) {
        return trp_empty_node();
    } else {
        /* return sibling */
        child_idx++;
        auto& sibl_name = ctx->row->second[child_idx];
        return get_node(sibl_name, ctx->tree.att_v);
    }
}

trt_node next_sibling_read(const struct trt_tree_ctx *ctx)
{
    struct trt_tree_ctx tmp = *ctx;
    tmp.row = tmp.tree.map.find(ctx->row->first);
    return next_sibling(&tmp);
}

trt_node next_child(struct trt_tree_ctx* ctx)
{
    if(ctx->row->second.empty())
        return trp_empty_node();

    auto& child_idx = ctx->child_idx;
    /* if pointing to parent */ 
    if(child_idx < 0) {
        /* get his child */ 
        child_idx = 0;
        auto node_name = ctx->row->second[child_idx];
        return get_node(node_name, ctx->tree.att_v);
    }
    /* else find child of child */ 
    auto& node_name = ctx->row->second[child_idx];
    tree_iter iter;
    /* find child as key in map */ 
    for(iter = ctx->tree.map.begin(); iter != ctx->tree.map.end(); ++iter) {
        if(node_name == iter->first) {
            break;
        }
    }

    if(iter != ctx->tree.map.end()) {
        /* try get child of child */ 
        /* if child has child */
        if(!iter->second.empty()){
            child_idx = 0;
            auto& node_name = iter->second[child_idx];
            ctx->row = iter;
            return get_node(node_name, ctx->tree.att_v);
        } else {
            /* child has no child */
            return trp_empty_node();
        }
    } else {
        /* child has no child */ 
        return trp_empty_node();
    }
}

trt_node parent(struct trt_tree_ctx* ctx)
{
    if(ctx->child_idx == -1) {
        /* search where is parent as children */
        auto& node_name = ctx->row->first;
        for(auto iter = ctx->tree.map.begin(); iter != ctx->tree.map.end(); ++iter) {
            for(auto const& item: iter->second) {
                if(item == node_name) {
                    ctx->row = iter;
                    ctx->child_idx = -1;
                    return get_node(iter->first, ctx->tree.att_v);
                }
            }
        }
        /* root node or bad tree */
        return trp_empty_node();
    } else {
        /* pointing to some child */
        ctx->child_idx = -1;
        return get_node(ctx->row->first, ctx->tree.att_v);
    }
}

#endif
