type BeamPath = { id: number; d: string; opacity: number; width: number };

function paths(direction: 1 | -1): BeamPath[] {
  return Array.from({ length: 12 }, (_, id) => {
    const shift = id * 28 * direction;
    const y = 36 + id * 31;
    return {
      id,
      d: `M ${-170 + shift} ${y} C ${100 + shift} ${y - 105} ${235 + shift} ${y + 150} ${422 + shift} ${y + 18} S ${670 + shift} ${y - 35} ${880 + shift} ${y + 90}`,
      opacity: 0.075 + id * 0.008,
      width: 0.65 + id * 0.035,
    };
  });
}

function DirectionalPaths({ direction }: { direction: 1 | -1 }) {
  const gradientId = `beamdrop-gradient-${direction}`;
  return <svg className="absolute inset-0 h-full w-full" fill="none" preserveAspectRatio="xMidYMid slice" viewBox="0 0 700 430">
    <defs>
      <linearGradient id={gradientId} x1="0" x2="1"><stop stopColor="#29d8ae" stopOpacity="0" /><stop offset="0.42" stopColor={direction === 1 ? "#62dbff" : "#a995ff"} /><stop offset="0.7" stopColor="#ffd084" /><stop offset="1" stopColor="#41e5c1" stopOpacity="0" /></linearGradient>
    </defs>
    {paths(direction).map((path) => <path key={path.id} d={path.d} stroke={`url(#${gradientId})`} strokeLinecap="round" strokeOpacity={path.opacity} strokeWidth={path.width} />)}
  </svg>;
}

/** 仅装饰用途的双向流光路径，不接收焦点或指针事件。 */
export function BackgroundPaths() {
  return <div aria-hidden="true" className="beamdrop-paths pointer-events-none fixed inset-0"><div className="beamdrop-orb beamdrop-orb-left" /><div className="beamdrop-orb beamdrop-orb-right" /><DirectionalPaths direction={1} /><DirectionalPaths direction={-1} /></div>;
}