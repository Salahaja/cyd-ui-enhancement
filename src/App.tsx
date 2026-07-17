import React, { useState, useEffect } from 'react';
import { 
  Wifi, 
  Battery, 
  Lightbulb, 
  Thermometer, 
  Lock, 
  ChevronRight, 
  Home, 
  Settings, 
  Clock,
  Sun,
  Moon
} from 'lucide-react';
import { motion, AnimatePresence } from 'framer-motion';

const SCREEN_WIDTH = 320;
const SCREEN_HEIGHT = 240;

const App = () => {
  const [time, setTime] = useState(new Date());
  const [activeTab, setActiveTab] = useState('home');

  useEffect(() => {
    const timer = setInterval(() => setTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  const formattedTime = time.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });

  return (
    <div className="flex flex-col items-center justify-center min-h-screen bg-zinc-900 p-4 font-sans select-none">
      {/* CYD Frame Simulation */}
      <div 
        className="relative bg-black rounded-[20px] p-4 shadow-2xl border-[12px] border-zinc-800 ring-1 ring-zinc-700"
        style={{ width: SCREEN_WIDTH + 48, height: SCREEN_HEIGHT + 48 }}
      >
        {/* Screen Area */}
        <div 
          className="relative overflow-hidden bg-black text-white rounded-sm"
          style={{ width: SCREEN_WIDTH, height: SCREEN_HEIGHT }}
        >
          {/* Status Bar */}
          <div className="absolute top-0 left-0 right-0 h-6 px-2 flex items-center justify-between bg-black/40 backdrop-blur-md z-50 text-[10px] font-medium tracking-tight">
            <span>{formattedTime}</span>
            <div className="flex items-center gap-1.5">
              <Wifi size={10} className="text-zinc-400" />
              <div className="flex items-center gap-0.5">
                <span className="text-zinc-400">84%</span>
                <Battery size={10} className="text-emerald-500 fill-emerald-500/20" />
              </div>
            </div>
          </div>

          {/* Main Content */}
          <main className="pt-6 h-full flex flex-col">
            <AnimatePresence mode="wait">
              {activeTab === 'home' && (
                <motion.div 
                  key="home"
                  initial={{ opacity: 0, x: 10 }}
                  animate={{ opacity: 1, x: 0 }}
                  exit={{ opacity: 0, x: -10 }}
                  className="p-3 flex-1 flex flex-col gap-3"
                >
                  <header>
                    <h1 className="text-lg font-semibold tracking-tight leading-tight">Welcome Home</h1>
                    <p className="text-[10px] text-zinc-500">Living Room • 21°C</p>
                  </header>

                  <div className="grid grid-cols-2 gap-2">
                    <GridItem 
                      icon={<Lightbulb size={16} />} 
                      label="Main Lights" 
                      status="On" 
                      active={true}
                      color="bg-amber-500"
                    />
                    <GridItem 
                      icon={<Thermometer size={16} />} 
                      label="AC Unit" 
                      status="Auto" 
                      active={false}
                      color="bg-blue-500"
                    />
                    <GridItem 
                      icon={<Lock size={16} />} 
                      label="Front Door" 
                      status="Locked" 
                      active={false}
                      color="bg-zinc-700"
                    />
                    <GridItem 
                      icon={<Sun size={16} />} 
                      label="Blinds" 
                      status="Open" 
                      active={true}
                      color="bg-zinc-700"
                    />
                  </div>
                </motion.div>
              )}

              {activeTab === 'settings' && (
                <motion.div 
                  key="settings"
                  initial={{ opacity: 0, x: 10 }}
                  animate={{ opacity: 1, x: 0 }}
                  exit={{ opacity: 0, x: -10 }}
                  className="p-3 flex-1"
                >
                  <h2 className="text-sm font-semibold mb-2">System Settings</h2>
                  <div className="space-y-1.5">
                    <SettingsRow label="Brightness" value="80%" />
                    <SettingsRow label="Wi-Fi" value="Connected" />
                    <SettingsRow label="Firmware" value="v1.0.4" />
                  </div>
                </motion.div>
              )}
            </AnimatePresence>

            {/* Bottom Nav */}
            <nav className="h-10 bg-zinc-900/80 backdrop-blur-md border-t border-zinc-800 flex items-center justify-around px-4">
              <NavButton 
                icon={<Home size={18} />} 
                active={activeTab === 'home'} 
                onClick={() => setActiveTab('home')} 
              />
              <NavButton 
                icon={<Clock size={18} />} 
                active={activeTab === 'time'} 
                onClick={() => setActiveTab('time')} 
              />
              <NavButton 
                icon={<Settings size={18} />} 
                active={activeTab === 'settings'} 
                onClick={() => setActiveTab('settings')} 
              />
            </nav>
          </main>
        </div>
        
        {/* Hardware details */}
        <div className="absolute -bottom-10 left-1/2 -translate-x-1/2 text-zinc-500 text-[10px] font-mono tracking-widest uppercase">
          CYD 320x240 Prototype
        </div>
      </div>
    </div>
  );
};

const GridItem = ({ icon, label, status, active, color }: any) => (
  <motion.div 
    whileTap={{ scale: 0.95 }}
    className={`p-2 rounded-xl flex flex-col justify-between h-[68px] transition-colors ${
      active ? color + ' text-white' : 'bg-zinc-800 text-zinc-400'
    }`}
  >
    <div className="flex justify-between items-start">
      <div className={active ? 'text-white' : 'text-zinc-500'}>
        {icon}
      </div>
      <ChevronRight size={10} />
    </div>
    <div>
      <p className="text-[10px] font-semibold leading-tight">{label}</p>
      <p className={`text-[8px] opacity-70`}>{status}</p>
    </div>
  </motion.div>
);

const SettingsRow = ({ label, value }: any) => (
  <div className="flex items-center justify-between p-2 rounded-lg bg-zinc-800/50 text-[11px]">
    <span className="text-zinc-400">{label}</span>
    <span className="text-zinc-100 font-medium">{value}</span>
  </div>
);

const NavButton = ({ icon, active, onClick }: any) => (
  <button 
    onClick={onClick}
    className={`p-1.5 rounded-full transition-all ${
      active ? 'bg-zinc-100 text-zinc-950' : 'text-zinc-500'
    }`}
  >
    {icon}
  </button>
);

export default App;
