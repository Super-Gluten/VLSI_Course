#include "Solution.h"
#include <set>
#include <unordered_set>

void Solution::initCompatibleCells() {
    site_compatible_cells_[SiteType::SLICE] = {"LUT1","LUT2","LUT3","LUT4","LUT5","LUT6","FDRE","CARRY8"};
    site_compatible_cells_[SiteType::DSP]   = {"DSP48E2"};
    site_compatible_cells_[SiteType::BRAM]  = {"RAMB36E2"};
    site_compatible_cells_[SiteType::IO]    = {"IBUF","OBUF","BUFGCE"};
}

bool Solution::isLegalPlacement(const ISPDInstance* inst, const ISPDSite* site) const {
    if (!site || !inst) return false;
    auto it = site_compatible_cells_.find(site->type);
    if (it == site_compatible_cells_.end()) return false;
    for (const auto& cell : it->second) if (cell == inst->cell_type) return true;
    return false;
}

int Solution::findFreeBEL(const ISPDSite* site, const ISPDInstance* inst) const {
    if (!site || !inst) return -1;
    if (site->type == SiteType::SLICE) {
        if (inst->isFF()) { for (int z=16; z<32; z++) if (site->isBELFree(z)) return z; }
        else if (inst->isLUT()) {
            if (inst->getLUTSize()==6) { for (int z=1; z<16; z+=2) if (site->isBELFree(z)&&site->isBELFree(z-1)) return z; }
            else { for (int z=0; z<16; z++) if (site->isBELFree(z)) return z; }
        } else { for (int z=0; z<32; z++) if (site->isBELFree(z)) return z; }
    } else if (site->type==SiteType::DSP || site->type==SiteType::BRAM) {
        return site->isBELFree(0)?0:-1;
    } else if (site->type==SiteType::IO) {
        for (int z=0; z<64; z++) if (site->isBELFree(z)) return z;
    }
    return -1;
}

bool Solution::searchNearbyBEL(int cx, int cy, const ISPDInstance* inst,
                               int& ox, int& oy, int& oz, int max_r) const {
    SiteType ct = SiteType::UNKNOWN;
    for (auto& kv : site_compatible_cells_)
        for (const auto& cell : kv.second)
            if (cell==inst->cell_type) { ct=kv.first; break; }
            else if (ct!=SiteType::UNKNOWN) break;
    if (ct==SiteType::UNKNOWN) return false;
    int W=db_->site_map_width, H=db_->site_map_height;
    for (int r=0; r<=max_r; r++) {
        for (int dx=-r; dx<=r; dx++) {
            int dy=r-std::abs(dx); if (dy<0) continue;
            for (int sy : {cy+dy, cy-dy}) {
                int sx=cx+dx; if (sx<0||sx>=W||sy<0||sy>=H) continue;
                ISPDSite* s=db_->site_map[sx][sy];
                if (!s||s->type!=ct) continue;
                int fz=findFreeBEL(s,inst);
                if (fz>=0) { ox=sx; oy=sy; oz=fz; return true; }
            }
        }
    }
    return false;
}

static SiteType getSiteTypeForCell(const std::string& cell) {
    if (cell=="LUT1"||cell=="LUT2"||cell=="LUT3"||cell=="LUT4"||cell=="LUT5"||cell=="LUT6"||cell=="FDRE"||cell=="CARRY8") return SiteType::SLICE;
    if (cell=="DSP48E2") return SiteType::DSP;
    if (cell=="RAMB36E2") return SiteType::BRAM;
    if (cell=="IBUF"||cell=="OBUF"||cell=="BUFGCE") return SiteType::IO;
    return SiteType::UNKNOWN;
}

bool Solution::constructInitialPlacement() {
    movable_insts_ = db_->getMovableInstances();
    int N = (int)movable_insts_.size();
    std::cout << "[INFO] Greedy placement: " << N << " instances ..." << std::endl;

    std::unordered_set<ISPDInstance*> placed;
    for (auto& kv : db_->instances) if (kv.second->isFixed()) placed.insert(kv.second);

    std::map<SiteType, std::vector<ISPDSite*>> sites_by_type;
    for (int x=0; x<db_->site_map_width; x++)
        for (int y=0; y<db_->site_map_height; y++) {
            ISPDSite* s=db_->site_map[x][y];
            if (s) sites_by_type[s->type].push_back(s);
        }
    std::map<SiteType, size_t> site_ptr;

    std::vector<ISPDNet*> all_nets;
    for (auto& kv : db_->nets) all_nets.push_back(kv.second);

    int placed_count = 0;
    int stagnant = 0;
    const int MAX_PASSES = 20;

    for (int pass=0; pass<MAX_PASSES; pass++) {
        int newly = 0;
        for (ISPDNet* net : all_nets) {
            int cx=0, cy=0, pn=0;
            std::vector<ISPDInstance*> unpl;
            for (const auto& pin : net->pins) {
                ISPDInstance* ii=pin.first;
                if (placed.count(ii)) { cx+=ii->x; cy+=ii->y; pn++; }
                else if (!ii->isFixed()) unpl.push_back(ii);
            }
            if (unpl.empty()||pn==0) continue;
            cx/=pn; cy/=pn;
            for (auto* inst : unpl) {
                if (placed.count(inst)) continue;
                SiteType st = getSiteTypeForCell(inst->cell_type);
                if (st==SiteType::UNKNOWN) continue;
                int nx, ny, nz;
                if (searchNearbyBEL(cx, cy, inst, nx, ny, nz, 10)) {
                    inst->x=nx; inst->y=ny; inst->z=nz;
                    db_->site_map[nx][ny]->bel_occupancy[nz]=inst;
                    placed.insert(inst); newly++;
                } else {
                    auto& sl=sites_by_type[st]; size_t& si=site_ptr[st];
                    for (size_t t=0; t<sl.size(); t++) {
                        int fz=findFreeBEL(sl[si],inst);
                        if (fz>=0) {
                            inst->x=sl[si]->x; inst->y=sl[si]->y; inst->z=fz;
                            sl[si]->bel_occupancy[fz]=inst;
                            placed.insert(inst); newly++; break;
                        }
                        si=(si+1)%sl.size();
                    }
                }
            }
        }
        placed_count+=newly;
        if (newly==0) { if (++stagnant>=3) break; } else stagnant=0;
        if (placed_count>=N) break;
    }

    // Fill remaining
    if (placed_count<N) {
        for (auto* inst : movable_insts_) {
            if (placed.count(inst)) continue;
            SiteType st=getSiteTypeForCell(inst->cell_type);
            if (st==SiteType::UNKNOWN) continue;
            auto& sl=sites_by_type[st]; size_t& si=site_ptr[st];
            for (size_t t=0; t<sl.size(); t++) {
                int fz=findFreeBEL(sl[si],inst);
                if (fz>=0) {
                    inst->x=sl[si]->x; inst->y=sl[si]->y; inst->z=fz;
                    sl[si]->bel_occupancy[fz]=inst; placed_count++; break;
                }
                si=(si+1)%sl.size();
            }
        }
    }

    std::cout << "[INFO] Greedy done: " << placed_count << "/" << N << std::endl;
    return placed_count>=N;
}

int Solution::computeNetsHPWL(const std::set<ISPDNet*>& nets) const {
    int t=0; for (auto* n : nets) t+=n->evalHPWL(); return t;
}

int Solution::computeTotalHPWL() const { return db_->computeTotalHPWL(); }

void Solution::doMove(ISPDInstance* inst, ISPDSite* ts, int tz) {
    ISPDSite* os=db_->site_map[inst->x][inst->y];
    if (os) { for (auto it=os->bel_occupancy.begin(); it!=os->bel_occupancy.end(); ++it) if (it->second==inst) { os->bel_occupancy.erase(it); break; } }
    inst->x=ts->x; inst->y=ts->y; inst->z=tz;
    ts->bel_occupancy[tz]=inst;
}

int Solution::solve() {
    if (!constructInitialPlacement()) { std::cerr<<"[ERROR] init failed\n"; return -1; }
    int N=(int)movable_insts_.size();
    if (N==0) return 0;

    total_hpwl_=computeTotalHPWL();
    best_hpwl_=total_hpwl_;
    initial_hpwl_=total_hpwl_;

    // Large cases: greedy only, skip SA
    if (N>100000) {
        std::cout<<"[INFO] Large N="<<N<<", greedy HPWL="<<total_hpwl_<<std::endl;
        return 0;
    }

    // SA for small cases
    double T0=total_hpwl_*0.02; if (T0<1) T0=100;
    double alpha=0.95;
    double T_min=T0*std::pow(alpha,200);
    int inner=N*5;

    std::cout<<"[INFO] SA: N="<<N<<", T0="<<(int)T0
             <<", inner="<<inner<<", HPWL="<<total_hpwl_<<std::endl;

    std::map<SiteType,std::vector<ISPDSite*>> sbt;
    for (int x=0; x<db_->site_map_width; x++)
        for (int y=0; y<db_->site_map_height; y++)
            { ISPDSite* s=db_->site_map[x][y]; if(s) sbt[s->type].push_back(s); }

    double T=T0; int last_report=0;
    while (T>T_min) {
        int acc=0, cr=0;
        for (int i=0; i<inner; i++) {
            iter_count_++;
            int idx=std::uniform_int_distribution<int>(0,N-1)(rng_);
            ISPDInstance* inst=movable_insts_[idx];
            SiteType ct=getSiteTypeForCell(inst->cell_type);
            if (ct==SiteType::UNKNOWN) continue;
            auto& sl=sbt[ct]; if (sl.empty()) continue;
            std::uniform_int_distribution<int> sd(0,(int)sl.size()-1);
            ISPDSite* ts=nullptr; int tz=-1;
            for (int t=0; t<10; t++) {
                ISPDSite* c=sl[sd(rng_)]; int fz=findFreeBEL(c,inst);
                if (fz>=0) { ts=c; tz=fz; break; }
            }
            if (!ts) { rejected_legal_count_++; cr++; continue; }
            std::set<ISPDNet*> aff; for (auto* n : inst->nets) aff.insert(n);
            int before=computeNetsHPWL(aff);
            int ox=inst->x, oy=inst->y, oz=inst->z;
            doMove(inst,ts,tz);
            int delta=computeNetsHPWL(aff)-before;
            if (delta<=0 || uni_dist_(rng_)<std::exp(-delta/T)) {
                total_hpwl_+=delta; acc++; cr=0;
                if (total_hpwl_<best_hpwl_) {
                    best_hpwl_=total_hpwl_;
                    if (iter_count_-last_report>20000) {
                        std::cout<<"[INFO] Best: "<<best_hpwl_<<" at "<<iter_count_<<std::endl;
                        last_report=iter_count_;
                    }
                }
            } else {
                ISPDSite* os=db_->site_map[ox][oy];
                if (os) { inst->x=ox; inst->y=oy; inst->z=oz; os->bel_occupancy[oz]=inst; }
                cr++;
            }
        }
        accepted_count_+=acc;
        if (cr>=N*20) break;
        T*=alpha;
    }
    std::cout<<"[INFO] SA done: best="<<best_hpwl_<<std::endl;
    return 0;
}
