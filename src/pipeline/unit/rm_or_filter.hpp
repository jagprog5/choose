#pragma once

#include "pipeline/unit/unit.hpp"
#include "utils/regex.hpp"

namespace choose {
namespace pipeline {

struct RmOrFilterOp : public PipelineUnit {
  enum Type { REMOVE, FILTER };
  Type type;
  regex::code arg;
  regex::match_data match_data;

  RmOrFilterOp(NextUnit next, Type type, regex::code&& arg)
      : //
        PipelineUnit(std::move(next)),
        type(type),
        arg(std::move(arg)),
        match_data(regex::create_match_data(this->arg)) {}

    void process(Packet&& p) override {
        this->completion_guard(p);
        
        const char* id = this->type == RmOrFilterOp::REMOVE ? "remove" : "filter";
        int rc = regex::match(this->arg, begin, end - begin, this->match_data, id);
    }
};



}
}

struct RmOrFilterOp {
  enum Type { REMOVE, FILTER };
  Type type;
  regex::code arg;
  regex::match_data match_data;

  RmOrFilterOp(Type type, regex::code&& arg)
      : type(type), //
        arg(std::move(arg)),
        match_data(regex::create_match_data(this->arg)) {}

  // returns true iff the token should not pass to the output
  bool removes(const char* begin, const char* end) const {
    const char* id = this->type == RmOrFilterOp::REMOVE ? "remove" : "filter";
    int rc = regex::match(this->arg, begin, end - begin, this->match_data, id);

    if (rc > 0) {
      // there was a match
      if (this->type == RmOrFilterOp::REMOVE) {
        return true;
      }
    } else {
      // there was no match
      if (this->type == RmOrFilterOp::FILTER) {
        return true;
      }
    }
    return false;
  }
};