import { Slot } from '@radix-ui/react-slot'
import { cva, type VariantProps } from 'class-variance-authority'
import type { ButtonHTMLAttributes } from 'react'
import { cn } from '../../lib/utils'

const buttonVariants = cva('inline-flex h-10 items-center justify-center gap-2 rounded-none px-4 text-sm font-medium transition-colors focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-primary disabled:pointer-events-none disabled:opacity-50', {
  variants: { variant: { default: 'bg-primary text-primary-foreground hover:bg-[#a69680]', outline: 'border border-border bg-card hover:bg-muted', ghost: 'hover:bg-muted', destructive: 'bg-destructive text-white hover:bg-[#8f1c13]' }, size: { default: '', sm: 'h-8 px-2 text-xs', icon: 'h-9 w-9 px-0' } },
  defaultVariants: { variant: 'default', size: 'default' },
})

export interface ButtonProps extends ButtonHTMLAttributes<HTMLButtonElement>, VariantProps<typeof buttonVariants> { asChild?: boolean }
export function Button({ className, variant, size, asChild = false, ...props }: ButtonProps) {
  const Comp = asChild ? Slot : 'button'
  return <Comp className={cn(buttonVariants({ variant, size }), className)} {...props} />
}